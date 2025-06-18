

#ifdef GEGL_PROPERTIES


#else

// Boilerplate code for a GEGL operation

// Declare is a op of type GEGL_OP_AREA_FILTER
// An area operation processes each pixel from surrounding pixels
#define GEGL_OP_AREA_FILTER
#define GEGL_OP_NAME     non_max_suppression
#define GEGL_OP_C_SOURCE non-max-suppression-op.c

// Base on the above definitions, gegl-op.h generates code for the operation
#include "gegl-op.h"

#include "non-max-suppression.h"



static void prepare (GeglOperation *operation)
{
  const Babl *space = gegl_operation_get_source_space (operation, "input");

  // Set the area filter's left, right, top, and bottom padding to 1 pixel.
  // This, in conjuntion withabyss policy, 
  // means the operation will consider a 1-pixel border around each pixel.
  // GeglOperationAreaFilter *area = GEGL_OPERATION_AREA_FILTER (operation);
  // area->left = area->right = area->top = area->bottom = 1;

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
         const GeglRectangle *rect,
         gint                 level)
{
  /* 
  If we were using an abyss policy, this is needed.

  Get a source rectangle that is required to compute the output.
  The rectangle is usually larger than the output rectangle,
  because it includes the padding around the output rectangle.
  Said padding is defined by the abyss policy.
  Said padding is produced by the gegl_buffer_get() call.

  GeglRectangle compute = gegl_operation_get_required_for_output (operation, "input", result);
  */

  //has_alpha = babl_format_has_alpha (gegl_operation_get_format (operation, "output"));
  
  g_debug ("%s in buffer format %s", G_STRFUNC, babl_format_get_encoding (gegl_buffer_get_format (input)));
  g_debug ("%s in op format %s", G_STRFUNC, babl_format_get_encoding (gegl_operation_get_format (operation, "input")));
  g_debug ("%s out op format %s", G_STRFUNC, babl_format_get_encoding (gegl_operation_get_format (operation, "output")));

  non_maximum_suppression (
    input,
    /* Using same rect for input and output. */
    rect, 
    output, 
    rect,
    /* 
    Using format of input buffer, which is 2 float.
    Using format of operation (Y'A) does not work, it gives 1.0 for direction.
    */
    gegl_buffer_get_format (input));

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
    "title",       "Non-Maximum Suppression",
    "name",        "bootch:non-max-suppression",
    "blurb",       "Thin edges in gradient field.",
    "version",     "0.1",
    "categories",  "Artistic",
    "description", "Non-maximum suppression in gradient field.",
    "author",      "Bootch",
    NULL);
}

#endif