

#ifdef GEGL_PROPERTIES


#else

// Boilerplate code for a GEGL operation

// Declare is a op of type GEGL_OP_AREA_FILTER
// An area operation processes each pixel from surrounding pixels
#define GEGL_OP_AREA_FILTER
#define GEGL_OP_NAME     my_area_op
#define GEGL_OP_C_SOURCE my-area-filter.c

// Base on the above definitions, gegl-op.h generates code for the operation
#include "gegl-op.h"


#define BPP 2 // Bytes per pixel for the input format (Y'A float has 2 channels)



/*
Define the clamped axis for gradient directions.

Clamped to the nearest 45 degrees.

The axis NS (North-South) is 0 degrees.
The direction of the gradient, 
either north to south or south to north,
is clamped to the same axis.
*/

typedef enum
{
  AXIS_NS   = 0,
  AXIS_NW_SE,
  AXIS_EW,
  AXIS_SW_NE
} DirectionAxis;


static DirectionAxis
clamped_gradient_axis (gfloat *gradient)
{
  gfloat angle = gradient[1]; // Assuming the second channel is the angle.
  
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
    return AXIS_SW_NE; // South-West to North-East
}

/*
Check if center is a local maximum magnitude
in the clamped direction of the gradient.

Local with respect to two neighbor pixel gradient magnitudes.

All arguments are pointers to pixels
having two channels: magnitude and direction.
*/
static gboolean
is_gradient_magnitude_a_local_maximum(
  gfloat *top_left,    gfloat *top,    gfloat *top_right,
  gfloat *left,        gfloat *center, gfloat *right,
  gfloat *bottom_left, gfloat *bottom, gfloat *bottom_right
)
{
  gboolean result = FALSE;

  switch ( clamped_gradient_axis (center) )
  {
    case AXIS_NS:
      // Is center is greater than its vertical neighbors?
      result = (center[0] > top[0] && center[0] > bottom[0]);
      break;
    case AXIS_NW_SE:
      // Is center is greater than its first diagonal neighbors?
      result = (center[0] > top_left[0] && center[0] > bottom_right[0]);
      break;
    case AXIS_EW:
      // Is center is greater than its horizontal neighbors?
      result = (center[0] > left[0] && center[0] > right[0]);
      break;
    case AXIS_SW_NE:
      // Is center is greater than its second diagonal neighbors?
      result = (center[0] > bottom_left[0] && center[0] > top_right[0]);
      break;
    default:
      g_error ( "Unexpected value of gradient direction");
      // If we reach here, programming error.
      result= FALSE;
  }
  
  return result;
}

static void
non_maximum_suppression
 (GeglBuffer          *src,
  const GeglRectangle *src_rect,
  GeglBuffer          *dst,
  const GeglRectangle *dst_rect,
  const Babl          *format)
{
  gint x,y;
  gint dest_index = 0;
  gfloat *src_buf;
  gfloat *after_src_buf;
  gfloat *dst_buf;

  src_buf = g_new0 (gfloat, src_rect->width * src_rect->height * BPP);
  dst_buf = g_new0 (gfloat, dst_rect->width * dst_rect->height * BPP);

  gegl_buffer_get (src, src_rect, 1.0, format,
                   src_buf, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);

  after_src_buf = src_buf + src_rect->width * src_rect->height * BPP;

  /*
  Raster scan the source rectangle.
  */
  for (y = 0; y < dst_rect->height; y++)
    for (x = 0; x < dst_rect->width; x++)
      {
        gfloat *row_start, *row_next;
        
        /* Pointers to neighbors. */
        gfloat *top_left,    *top,     *top_right;
        gfloat *left,        *center,  *right;
        gfloat *bottom_left, *bottom,  *bottom_right;

        row_start = src_buf + y * src_rect->width * BPP;
        row_next = row_start + src_rect->width * BPP;

        /* 
        Compute pointers to neighbor pixels.
        Using address arithmetic.
        Clamping pointers into the buf.
        */
        /* Unclamped pointers to neighbor pixels. */
      
        center       = row_start + x * BPP;
        left         = center - BPP;
        right        = center + BPP;

        top          = center - src_rect->width * BPP;
        top_left     = top - BPP;
        top_right    = top + BPP;
        
        bottom       = center + src_rect->width * BPP;
        bottom_left  = bottom - BPP;
        bottom_right = bottom + BPP;

        /* 
        When pointers to neighbors are out of source rect bounds,
        clamp them into the rect.
        */
        if (top < src_buf)
          {
            top_left  += src_rect->width * BPP;
            top       += src_rect->width * BPP;
            top_right += src_rect->width * BPP;
          }
        else if (bottom >= after_src_buf)
          {
            bottom_left  -= src_rect->width * BPP;
            bottom       -= src_rect->width * BPP;
            bottom_right -= src_rect->width * BPP;
          }

        if (left < row_start)
          {
            top_left    += BPP;
            left        += BPP;
            bottom_left += BPP;
          }
        else if (right >= row_next)
          {
            top_right    -= BPP;
            right        -= BPP;
            bottom_right -= BPP;
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
            dst_buf[dest_index] = 0;
          }

        // Keep direction component, unchanged.
        dst_buf[dest_index + 1] = center[1];

        dest_index++;
      }

  gegl_buffer_set (dst, dst_rect, 0, format, dst_buf,
                   GEGL_AUTO_ROWSTRIDE);
  g_free (src_buf);
  g_free (dst_buf);
}


static void prepare (GeglOperation *operation)
{
  const Babl *space = gegl_operation_get_source_space (operation, "input");
  GeglOperationAreaFilter *area = GEGL_OPERATION_AREA_FILTER (operation);

  // Set the area filter's left, right, top, and bottom padding to 1 pixel.
  // This means the operation will consider a 1-pixel border around each pixel.
  area->left = area->right = area->top = area->bottom = 1;

  // Set the input and output formats to Y'A float with the specified space.
  // Y' is the magnitude channel, A is the direction channel.
  gegl_operation_set_format (operation, "input",  babl_format_with_space ("Y'A float", space));
  gegl_operation_set_format (operation, "output", babl_format_with_space ("Y'A float", space));
}


/* Has type of FilterClass.Process */
static gboolean
process (GeglOperation       *operation,
         GeglBuffer          *input,
         GeglBuffer          *output,
         const GeglRectangle *result,
         gint                 level)
{
  GeglRectangle compute;
  

  compute = gegl_operation_get_required_for_output (operation, "input", result);
  //has_alpha = babl_format_has_alpha (gegl_operation_get_format (operation, "output"));
  
  non_maximum_suppression (
    input,
    &compute, 
    output, 
    result,
    babl_format_with_space ("RGBA float",
    gegl_operation_get_format (operation, "output")));

  return TRUE;
}

static void
gegl_op_class_init (GeglOpClass *klass)
{
  // The base class of all GEGL operations is GeglOperation.
  GeglOperationClass *operation_class = GEGL_OPERATION_CLASS (klass);

  // The subclass for area filters is GeglOperationAreaFilterClass.
  // It specializes, having a process method that is called for each pixel.
  //GeglOperationAreaFilterClass *area_filter_class = GEGL_OPERATION_AREA_FILTER_CLASS (klass);

  // The parent class for area filters is GeglOperationFilterClass.
  GeglOperationFilterClass *filter_class = GEGL_OPERATION_FILTER_CLASS (klass);
  
  
  // Override superclass methods.
  operation_class->prepare = prepare;
  filter_class->process    = process;

  // Set the abyss policy for this operation.
  // operation_class->get_abyss_policy = gegl_operation_area_filter_get_abyss_policy;

  // Set the operation class attributes/properties.
  operation_class->opencl_support = FALSE;
  operation_class->threaded       = FALSE;

  gegl_operation_class_set_keys (operation_class,
    "title",       "A Area filter",
    "name",        "bootch:my-area-filter",
    "blurb",       "A simple area gegl filter",
    "version",     "0.1",
    "categories",  "Artistic",
    "description", "my area gegl filter",
    "author",      "Bootch",
    NULL);
}

#endif