/*
A brushfire operation.
Historically called hysteresis, but that is a misnomer.
Brushfire is a more accurate name.

Promotes white pixels connected to white pixels, to white.

Used in Canny edge detection to promote weak edges to strong edges.

The fire spreads from white pixels (strong) to connected pixels
that are not black (weak.)

This is a brute-force implementation (many iterations over the image.)
Other implementations may be more efficient.

The input is two channels.
Only the first channel is used and changed.
The first channel is in range [0, 1].
The first channel typically came from magnitude of a gradient vector.
Subsequent interpretation is typically as luminance (Y').

When used on its own, on a typical image that is not "edges",
the filter has the effect of making large white areas,
everywhere that was not originally pure black,
or an area surrounded by pure black and having no white pixels.

This is more general than a simple threshold,
because it considers connectness of pixels.

The operation could be generalized to promote in the opposite direction,
promoting non-white pixels to black where connected to black pixels.
*/


#ifdef GEGL_PROPERTIES


#else

// Boilerplate code for a GEGL operation

// Declare is a op of type GEGL_OP_AREA_FILTER
// An area operation processes each pixel from surrounding pixels
#define GEGL_OP_AREA_FILTER
#define GEGL_OP_NAME     hysteresis
#define GEGL_OP_C_SOURCE hysteresis-op.c

// Base on the above definitions, gegl-op.h generates code for the operation
#include "gegl-op.h"

#include "hysteresis.h"



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
  
  hysteresis (
    input,
    /* Using same rect for input and output. */
    rect, 
    output, 
    rect,
    /* Using same format */
    gegl_operation_get_format (operation, "output"));

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
    "title",       "Promote White Connected Pixels",
    "name",        "bootchk:hysteresis",
    "blurb",       "Promote weak edges to strong edges.",
    "version",     "0.1",
    "categories",  "Artistic",
    "description", "Promote pixels connected to white, to white.",
    "author",      "lloyd konneker",
    NULL);
}

#endif