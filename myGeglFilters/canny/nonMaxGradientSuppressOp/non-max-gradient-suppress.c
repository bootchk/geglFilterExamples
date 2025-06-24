
#include <gegl.h>

#include "non-max-gradient-suppress.h"




#define FPP 2 // Floats per pixel for the input format (has 2 channels)


/*
Define the clamped axis for gradient directions.

Clamp gradient vectors to the nearest 45 degree axis.

The axis NS (North-South) is 0 degrees.
There are four axes (not eight).

Up and down vectors, north to south and south to north, are clamped to the same axis.
*/

typedef enum
{
  AXIS_NS   = 0,
  AXIS_NW_SE,
  AXIS_EW,
  AXIS_SW_NE
} DirectionAxis;


/*
Gradient is two channels, second is an angle in radians [-pi, pi],
using the East-Counterclockwise Convention
(0 degrees is East, 90 degrees is North, 180 degrees is West, -90 degrees is South)
i.e. computed by atan2 (dy, dx).

The implementation is not efficient, but it is simple and clear.
That is, we don't need to convert convention, normalize, or work in degrees,
except for intuitive understanding of the result.
*/
static DirectionAxis
clamped_axis_of_vector (gfloat *vector)
{
  gfloat angle = vector[1];

  //if (angle != 1.0)
  //  g_debug ("%s: angle in radians = %f", G_STRFUNC, angle);

  // Normalize angle to [0, 2 pi) degrees.
  if (angle < 0)
    angle += 2 * G_PI; // Convert negative angle to positive.
  else if (angle >= 2 * G_PI)
    angle -= 2 * G_PI; // Convert angle greater than 2 pi  to [0, 2 pi).

  // Convert radians to degrees.
  angle = angle * 180.0 / G_PI;

  //g_debug ("%s: angle in degrees = %f", G_STRFUNC, angle);

  // Remember that the angle is in East-Counterclockwise Convention.
  if (angle < 22.5 || angle >= 337.5)
    return AXIS_EW;
  else if (angle >= 22.5 && angle < 67.5)
    return AXIS_NW_SE;
  else if (angle >= 67.5 && angle < 112.5)
    return AXIS_NS;
  else if (angle >= 112.5 && angle < 157.5)
    return AXIS_SW_NE;
  else if (angle >= 157.5 && angle < 202.5)
    return AXIS_EW;
  else if (angle >= 202.5 && angle < 247.5)
    return AXIS_NW_SE;
  else if (angle >= 247.5 && angle < 292.5)
    return AXIS_NS;
  else
    // angle >= 292.5 && angle < 337.5
    return AXIS_SW_NE;
}

/*
Check if center is a local maximum magnitude
in the clamped direction of the gradient.

Local with respect to two neighbor pixel gradient magnitudes.

All arguments are pointers to pixels
having two channels: magnitude and direction,
representing a vector at that pixel.
*/
static gboolean
is_gradient_magnitude_a_local_maximum(
  gfloat *top_left,    gfloat *top,    gfloat *top_right,
  gfloat *left,        gfloat *center, gfloat *right,
  gfloat *bottom_left, gfloat *bottom, gfloat *bottom_right
)
{
  gboolean result = FALSE;

  // g_debug ("%s", G_STRFUNC);

  switch ( clamped_axis_of_vector (center) )
  {
    // Is center magnitude greater than its...
    case AXIS_NS:
      // vertical neighbors?
      result = (center[0] > top[0] && center[0] > bottom[0]);
      break;
    case AXIS_NW_SE:
      // first diagonal neighbors?
      result = (center[0] > top_left[0] && center[0] > bottom_right[0]);
      break;
    case AXIS_EW:
      // horizontal neighbors?
      result = (center[0] > left[0] && center[0] > right[0]);
      break;
    case AXIS_SW_NE:
      // second diagonal neighbors?
      result = (center[0] > bottom_left[0] && center[0] > top_right[0]);
      break;
    default:
      g_error ( "Unexpected value of gradient direction");
      // If we reach here, programming error.
      result= FALSE;
  }
  
  return result;
}

/*
Src and dst are format YA.
Interpreted as a gradient field: an array of vectors.  
A vector has two components, magnitude and direction.

The source and destination rectangles are NOT the same size.
The source rectangle is larger, using an abyss policy
to initialze the extra pixels in the source buffer.
The extra pixels a one pixel border around the destination rectangle.

The coordinate systems are not the same.
See below, converting from source to destination.
*/
void
non_maximum_suppression
 (GeglBuffer          *src,
  const GeglRectangle *src_rect,
  GeglBuffer          *dst,
  const GeglRectangle *dst_rect,
  const Babl          *format)
{
  gfloat *src_buf, *dst_buf;

  // 

  g_debug ("%s", G_STRFUNC);

  g_debug ("src_rect: %d x %d, dst_rect: %d x %d",
           src_rect->width, src_rect->height,
           dst_rect->width, dst_rect->height);

  if (src_rect->width <= 0 || src_rect->height <= 0)
    return; // Nothing to process.

  // Different size
  src_buf = g_new0 (gfloat, src_rect->width * src_rect->height * FPP);
  dst_buf = g_new0 (gfloat, dst_rect->width * dst_rect->height * FPP);

  /* 
  Abyss policy "clamp" initializes extra, border pixels in source to nearest actual source pixel value.
  When gradient direction is orthogonal across the edge of the image,
  an edge pixel will never be a local maximum
  since it will have a neighbor equal to itself.
  When gradient direction is along the edge of the image,
  or diagonal across the edge of the image,
  an edge pixel can be a local maximum and can be thinned.
  */

  gegl_buffer_get (src, src_rect, 1.0,
    /* Operation only allows one format, same as set on operation. */
    format,
    src_buf, GEGL_AUTO_ROWSTRIDE,
    GEGL_ABYSS_CLAMP);

  g_debug ("%s before scan", G_STRFUNC);

  /*
  Raster scan the dest rectangle.
  Derived from edge-sobel.c
  */ 
  {
    guint dest_col, dest_row;
    gint dest_index = 0;

  for (dest_row = 0; dest_row < dst_rect->height; dest_row++)
    {
      // gfloat *dest_row_start = src_buf + dest_row * src_rect->width * FPP;
     
      /*
      Start of source row is one row past dest_row. 
      The first row is a row of extra pixels, artificial neighbors above the second row.
      */
      gfloat *source_row_start_ptr = src_buf + (dest_row + 1)  * src_rect->width * FPP;

      for (dest_col = 0; dest_col < dst_rect->width; dest_col++)
        {
          /* Pointers to neighbors. */
          gfloat *top_left,    *top,     *top_right;
          gfloat *left,        *center,  *right;
          gfloat *bottom_left, *bottom,  *bottom_right;

          /* 
          Compute pointers to neighbor pixels in source buffer.
          Using address arithmetic.
          Assert calculated pointers are in bounds of source rect.
          */

          /* source_col_index is one pixel more than dest col. */
          gint source_col_index = dest_col + 1;

          center       = source_row_start_ptr + source_col_index * FPP;

          /* Left and right are one pixel previous and following. */
          left         = center - FPP;
          right        = center + FPP;

          /* Top is one row width previous. */
          top          = center - src_rect->width * FPP;
          top_left     = top - FPP;
          top_right    = top + FPP;
          
          bottom       = center + src_rect->width * FPP;
          bottom_left  = bottom - FPP;
          bottom_right = bottom + FPP;

          /* Assert pointers to neighbors are in bounds of source rect */
          
          /*
          Perform the filtering, to the output buffer.
          When center is local maximum, 
          keep magnitude component,
          else discard (set to zero).
          */
          if (is_gradient_magnitude_a_local_maximum(
                top_left, top, top_right,
                left, center, right,
                bottom_left, bottom, bottom_right))
            {
              // keep its magnitude component.
              dst_buf[dest_index] = center[0];
            } 
          else 
            {
              dst_buf[dest_index] = 0;  // viewed as black
            }

          // Keep direction component, unchanged.
          dst_buf[dest_index + 1] = center[1];

          // Increment the destination index.
          dest_index += FPP;

          // g_debug ("dst_buf[%d] = %f", dest_index, dst_buf[dest_index]);
        } // End of inner loop over dest_col
    } // End of outer loop over dest_row
  } // End of raster scan.

  g_debug ("%s after scan", G_STRFUNC);

  // Set the destination buffer with the processed data.
  gegl_buffer_set (dst, dst_rect, 0, format, dst_buf,
                   GEGL_AUTO_ROWSTRIDE);
  g_free (src_buf);
  g_free (dst_buf);
}
