
#include <gegl.h>

#include "hysteresis.h"




#define FPP 2 // Floats per pixel for the input format (Y'A float has 2 channels)

/* Does pixel have magnitude with criteria "strong" */
#define is_strong_neighbor(pixel) \
  ((pixel[0] > 0.5))

/*
Check if center is connected to strong edges/neighbors.
8-connected neighborhood (alternative is 4 or 6 connected).
Returns TRUE if any neighbor is strong.
*/
static gboolean
is_connected_to_strong(
  gfloat *top_left,    gfloat *top,    gfloat *top_right,
  gfloat *left,        gfloat *center, gfloat *right,
  gfloat *bottom_left, gfloat *bottom, gfloat *bottom_right
)
{
  // assert center is weak, we don't check it here.
  gboolean result = 
    is_strong_neighbor(top_left) ||
    is_strong_neighbor(top) ||
    is_strong_neighbor(top_right) ||
    is_strong_neighbor(left) ||
    // not check is_strong_neighbor(center) ||
    is_strong_neighbor(right) ||
    is_strong_neighbor(bottom_left) ||
    is_strong_neighbor(bottom) ||
    is_strong_neighbor(bottom_right);

  return result;
}

/*
A brushfire operation.
Raster scan the destination rectangle.
Promotes weak pixels to strong pixels
if they are connected to strong neighbors.
The fire is strongness burning through connected weak pixels.

Mutates the src buffer.
Was initialized from the input buffer,
but since mutated repeatedly.

Returns whether fire advanced,
i.e. some pixel was promoted from weak to strong.
*/
static gboolean
brushfire (
  gfloat              *src_buf,
  const GeglRectangle *src_rect
)
{
  guint col, row;
  // Used to clamp pointers to neighbors.
  gfloat *past_src_buf = src_buf + src_rect->width * src_rect->height * FPP;
  gboolean promoted = FALSE;
  guint promoted_count = 0;

  for (row = 0; row < src_rect->height; row++)
    {
      gfloat *row_start = src_buf + row * src_rect->width * FPP;
      gfloat *row_next  = row_start + src_rect->width * FPP;

      for (col = 0; col < src_rect->width; col++)
        {
          /* Pointers to neighbors. */
          gfloat *top_left,    *top,     *top_right;
          gfloat *left,        *center,  *right;
          gfloat *bottom_left, *bottom,  *bottom_right;

          center  = row_start + col * FPP;

          /*
          Promoting center weak to strong, scanning for weak pixels.
          Alternate design would scan for strong pixels, 
          and promote neighbors weak to strong.
          But this is might be more efficient.
          We anticipate edges are thin, with few neighbors to promote.
          */
          if (center[0] == 1.0 ||  // Strong, skip. (represents white, edge detected)
              center[0] <= 0.0     // Zero, skip. (represents black, no edge detected)
              )
            {
              // Not weak, skip.
              continue;
            }

          /* 
          Compute pointers to neighbor pixels.
          Using address arithmetic.
          Clamping pointers into the buf.
          */

          /* Unclamped pointers to neighbor pixels. */
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
          else if (bottom >= past_src_buf)
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
          
          */
          if (is_connected_to_strong(
                top_left, top, top_right,
                left, center, right,
                bottom_left, bottom, bottom_right))
            {
              // Promote to strong. Magnitude becomes max, viewed as white.
              center[0] = 1.0;
              promoted_count++;
              promoted = TRUE; // Fire advanced, a pixel was promoted.
            } 
          // else remains zero or weak. 

          // g_debug ("dst_buf[%d] = %f", dest_index, dst_buf[dest_index]);
        } // End of inner loop over col
    } // End of outer loop over row

  g_debug ("%s: promoted %d pixels", G_STRFUNC, promoted_count);

  // Return whether any pixel was promoted.
  return promoted;
} // End of brushfire function


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
hysteresis
 (GeglBuffer          *src,
  const GeglRectangle *src_rect,
  GeglBuffer          *dst,
  const GeglRectangle *dst_rect,
  const Babl          *format)
{
  gfloat *src_buf;  // array

  // Require the source and destination rectangles are the same size.
  g_return_if_fail (src_rect->width == dst_rect->width &&
                    src_rect->height == dst_rect->height);

  g_debug ("%s", G_STRFUNC);

  if (src_rect->width <= 0 || src_rect->height <= 0)
    return; // Nothing to process.

  {
    guint size = src_rect->width * src_rect->height * FPP;
  
    src_buf = g_new0 (gfloat, size);
    g_return_if_fail (src_buf != NULL );
  }

  gegl_buffer_get (src, src_rect, 1.0,
    /* Operation only allows one format, same as set on operation. */
    format,
    src_buf, GEGL_AUTO_ROWSTRIDE,
    /* Abyss policy not "clamp" because we are doing our own clamping.*/
    GEGL_ABYSS_NONE);

  // The brush fire loop continues until no more pixels are promoted.
  while (brushfire (src_buf, src_rect)) {}

  g_debug ("%s after brush fire loop", G_STRFUNC);

  // Set destination buffer with processed data, mutated src_buf!!!
  gegl_buffer_set (dst, dst_rect, 0, format, src_buf,
                   GEGL_AUTO_ROWSTRIDE);
  g_free (src_buf);
}
