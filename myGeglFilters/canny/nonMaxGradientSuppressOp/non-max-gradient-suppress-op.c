

#ifdef GEGL_PROPERTIES


#else

// Boilerplate code for a GEGL operation

// Declare is a op of type GEGL_OP_AREA_FILTER
// An area operation processes each pixel from surrounding pixels
#define GEGL_OP_AREA_FILTER
#define GEGL_OP_NAME     non_max_gradient_suppress
#define GEGL_OP_C_SOURCE non-max-gradient-suppress-op.c

// Base on the above definitions, gegl-op.h generates code for the operation
#include "gegl-op.h"

#include "non-max-gradient-suppress.h"



static void prepare (GeglOperation *operation)
{
  /* 
  Format is float[2],  magnitude and direction channels.
  Not colors, not having a colorspace.
  */
  const Babl *gradient_format= babl_format_n (babl_type ("float"), 2);

  /*
  Set the area filter's left, right, top, and bottom padding to 1 pixel.
  In conjunction with abyss policy, the op will use artificial neighbors
  in a 1-pixel border around the actual source.
  */ 
  GeglOperationAreaFilter *area = GEGL_OPERATION_AREA_FILTER (operation);
  area->left = area->right = area->top = area->bottom = 1;

  gegl_operation_set_format (operation, "input",  gradient_format);
  gegl_operation_set_format (operation, "output", gradient_format);
}


/* Has type of FilterClass.Process */
static gboolean
process (GeglOperation       *operation,
         GeglBuffer          *input,
         GeglBuffer          *output,
         const GeglRectangle *out_rect,
         gint                 level)
{
  /* 
  Get a source rectangle required to compute the output.
  The rectangle is larger than the output rectangle,
  because it includes the padding around the output rectangle.
  Said padding is initialized by an abyss policy.
  Said padding is produced by the gegl_buffer_get() call.
  */
  GeglRectangle computed_in_rect = gegl_operation_get_required_for_output (operation, "input", out_rect);
  
  g_debug ("%s in buffer format %s", G_STRFUNC, babl_format_get_encoding (gegl_buffer_get_format (input)));
  g_debug ("%s in op format %s", G_STRFUNC, babl_format_get_encoding (gegl_operation_get_format (operation, "input")));
  g_debug ("%s out op format %s", G_STRFUNC, babl_format_get_encoding (gegl_operation_get_format (operation, "output")));

  non_maximum_suppression (
    input,
    &computed_in_rect,  // input rectangle, larger than the output rectangle
    output, 
    out_rect,
    /* 
    Using format of input buffer, which is float[2],
    Using format (Y'A) does not work, it gives 1.0 for direction.
    */
    gegl_buffer_get_format (input));

  return TRUE;
}

static void
gegl_op_class_init (GeglOpClass *klass)
{
  // base class
  GeglOperationClass *operation_class = GEGL_OPERATION_CLASS (klass);

  // parent class 
  GeglOperationFilterClass *filter_class = GEGL_OPERATION_FILTER_CLASS (klass);
  
  // Override superclass methods.
  operation_class->prepare = prepare;
  filter_class->process    = process;

  /* Not set abyss policy for this op, we specify it explicitly in gegl_op_get_buffer(). */

  // Set the operation class attributes/properties.
  operation_class->opencl_support = FALSE;
  operation_class->threaded       = FALSE;

  gegl_operation_class_set_keys (operation_class,
    "title",       "Non-Max Gradient Suppress",
    "name",        "bootchk:non-max-gradient-suppress",
    "blurb",       "Thin edges in gradient field.",
    "version",     "0.1",
    "categories",  "edge-thinning",
    "description", "Non-maximum suppression of gradient field.",
    "author",      "lloyd konneker",
    NULL);
}

#endif