
#include <gegl.h>

#include "non-max-suppression.h"




#define FPP 2 // Floats per pixel for the input format (Y'A float has 2 channels)


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
Gradient is two channels, second is an angle in radians [-pi, pi].
*/
static DirectionAxis
clamped_axis_of_vector (gfloat *vector)
{
  gfloat angle = vector[1];

  if (angle != 1.0)
    g_debug ("%s: angle in radians = %f", G_STRFUNC, angle);

  // Normalize angle to [0, 2 pi) degrees.
  if (angle < 0)
    angle += 2 * G_PI; // Convert negative angle to positive.
  else if (angle >= 2 * G_PI)
    angle -= 2 * G_PI; // Convert angle greater than 2 pi  to [0, 2 pi).

  // Convert radians to degrees.
  angle = angle * 180.0 / G_PI;

  //g_debug ("%s: angle in degrees = %f", G_STRFUNC, angle);

  if (angle < 22.5 || angle >= 337.5)
    return AXIS_NS; // North-South
  else if (angle >= 22.5 && angle < 67.5)
    return AXIS_NW_SE; // North-West to South-East
  else if (angle >= 67.5 && angle < 112.5)
    return AXIS_EW; // East-West
  else if (angle >= 112.5 && angle < 157.5)
    return AXIS_SW_NE; // South-West to North-East
  else if (angle >= 157.5 && angle < 202.5)
    return AXIS_NS; // North-South
  else if (angle >= 202.5 && angle < 247.5)
    return AXIS_NW_SE; // North-West to South-East
  else if (angle >= 247.5 && angle < 292.5)
    return AXIS_EW; // East-West
  else
    // angle >= 292.5 && angle < 337.5
    return AXIS_SW_NE; // South-West to North-East
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

Requires the source and destination rectangles to be the same size.
The strategy here is to clamp pointers to neighbors back
into the source rectangle, if they are out of bounds.

Other strategies could be to use a different abyss policy?
*/
void
non_maximum_suppression
 (GeglBuffer          *src,
  const GeglRectangle *src_rect,
  GeglBuffer          *dst,
  const GeglRectangle *dst_rect,
  const Babl          *format)
{
  gfloat *src_buf, *after_src_buf, *dst_buf;

  // Require the source and destination rectangles are the same size.
  g_return_if_fail (src_rect->width == dst_rect->width &&
                    src_rect->height == dst_rect->height);

  g_debug ("%s", G_STRFUNC);

  g_debug ("src_rect: %d x %d, dst_rect: %d x %d",
           src_rect->width, src_rect->height,
           dst_rect->width, dst_rect->height);

  if (src_rect->width <= 0 || src_rect->height <= 0)
    return; // Nothing to process.

  // Same size
  src_buf = g_new0 (gfloat, src_rect->width * src_rect->height * FPP);
  dst_buf = g_new0 (gfloat, dst_rect->width * dst_rect->height * FPP);

  gegl_buffer_get (src, src_rect, 1.0,
    /* Operation only allows one format, same as set on operation. */
    format,
    src_buf, GEGL_AUTO_ROWSTRIDE,
    /* Abyss policy not "clamp" because we are doing our own clamping.*/
    GEGL_ABYSS_NONE);

  // Pointer to the end of the source buffer.
  // This is used to clamp pointers to neighbors.
  after_src_buf = src_buf + src_rect->width * src_rect->height * FPP;

  g_debug ("%s before scan", G_STRFUNC);

  /*
  Raster scan the source rectangle.
  Derived from edge-sobel.c
  */ 
  {
    guint col, row;
    gint dest_index = 0;

  /* 
  !!! Note we use dimensions of dst_rct.
  This seems like a relic of the original code.
  More generally, the src_rect is larger via an abyss policy.
   */
  for (row = 0; row < dst_rect->height; row++)
    {
      gfloat *row_start = src_buf + row * src_rect->width * FPP;
      gfloat *row_next  = row_start + src_rect->width * FPP;

      for (col = 0; col < dst_rect->width; col++)
        {
          /* Pointers to neighbors. */
          gfloat *top_left,    *top,     *top_right;
          gfloat *left,        *center,  *right;
          gfloat *bottom_left, *bottom,  *bottom_right;

          /* 
          Compute pointers to neighbor pixels.
          Using address arithmetic.
          Clamping pointers into the buf.
          */

          /* Unclamped pointers to neighbor pixels. */
          center       = row_start + col * FPP;
          left         = center - FPP;
          right        = center + FPP;

          top          = center - src_rect->width * FPP;
          top_left     = top - FPP;
          top_right    = top + FPP;
          
          bottom       = center + src_rect->width * FPP;
          bottom_left  = bottom - FPP;
          bottom_right = bottom + FPP;

          /* 
          When pointers to neighbors are out of source rect bounds,
          clamp them into the rect.
          */
          if (top < src_buf)
            {
              top_left  += src_rect->width * FPP;
              top       += src_rect->width * FPP;
              top_right += src_rect->width * FPP;
            }
          else if (bottom >= after_src_buf)
            {
              bottom_left  -= src_rect->width * FPP;
              bottom       -= src_rect->width * FPP;
              bottom_right -= src_rect->width * FPP;
            }

          if (left < row_start)
            {
              top_left    += FPP;
              left        += FPP;
              bottom_left += FPP;
            }
          else if (right >= row_next)
            {
              top_right    -= FPP;
              right        -= FPP;
              bottom_right -= FPP;
            }

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
        } // End of inner loop over col
    } // End of outer loop over row
  } // End of raster scan.

  g_debug ("%s after scan", G_STRFUNC);

  // Set the destination buffer with the processed data.
  gegl_buffer_set (dst, dst_rect, 0, format, dst_buf,
                   GEGL_AUTO_ROWSTRIDE);
  g_free (src_buf);
  g_free (dst_buf);
}
