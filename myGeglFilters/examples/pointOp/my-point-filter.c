#ifdef GEGL_PROPERTIES

property_double (low_threshold, "Low Threshold", 0.33)
    value_range (-200, 200)
    ui_range    (-1, 2)
    description("Values below this become black.")

property_double (high_threshold, "High Threshold", 0.66)
    value_range (-200, 200)
    ui_range    (0, 1)
    description("Values above this become white.")

#else

// Boilerplate code for a GEGL operation

// Declare is a op of type GEGL_OP_POINT_FILTER
// This is a point operation, i.e. it processes each pixel independently.
#define GEGL_OP_POINT_FILTER
#define GEGL_OP_NAME     my_point_op
#define GEGL_OP_C_SOURCE my-point-filter.c

// Base on the above definitions, gegl-op.h generates code for the operation
#include "gegl-op.h"


static void prepare (GeglOperation *operation)
{
  const Babl *space = gegl_operation_get_source_space (operation, "input");
  gegl_operation_set_format (operation, "input",  babl_format_with_space ("Y'A float", space));
  gegl_operation_set_format (operation, "output", babl_format_with_space ("Y'A float", space));
}


/* Transform function is
 *     ___
 *  __/
 *
 * Iterate over each pixel in the input buffer.
 * 
 * Each pixel is represented by two channels: Y' and A.
 * Y' is the luminance channel, A is the alpha channel.
 * 
 * The input buffer is expected to be in the format Y'A float.
 * The output buffer is also in the format Y'A float.
 * * The input and output buffers have two channels per pixel, hence the stride of 2.
 * 
 * The operation processes each pixel independently, hence it is a point filter.
 * Process each pixel in a single pass.
 * 
 * The Y' value is compared against two thresholds:
 * The output pixel is set to black (0) if the Y' value is below the low threshold,
 * set to white (1) if above the high threshold, and otherwise keeps the original Y' value.
 * 
 * The alpha channel is copied unchanged.
 * 
 * This is a simple thresholding operation that can be used for various effects,
 * such as creating a binary mask or isolating certain brightness levels in an image.
 */
static gboolean
process (GeglOperation       *op,
         void                *in_buf,
         void                *out_buf,
         glong                n_pixels,
         const GeglRectangle *roi,
         gint                 level)
{
  // Pointers to scan buffer.
  gfloat *in = in_buf;
  gfloat *out = out_buf;

  // Get low and high thresholds from the operation properties.
  gfloat low_threshold  = GEGL_PROPERTIES (op)->low_threshold;
  gfloat high_threshold = GEGL_PROPERTIES (op)->high_threshold;
  
  
  for (glong i=0; i<n_pixels; i++)
    {
      gfloat c = in[0];
  
      if (c < low_threshold)
        out[0] = 0; // Set to black when below low threshold
      else if (c > high_threshold)
        out[0] = 1; // Set to white when above high threshold
      else  // Otherwise, keep original value
        out[0] = in[0];
          
      // Copy second channel (if exists) unchanged.
      // Second channel is often alpha but could be other data.
      out[1] = in[1];

      // Move to the next pixel.
      in  += 2;
      out += 2;
    }

  return TRUE;
}


static void
gegl_op_class_init (GeglOpClass *klass)
{
  // The base class of all GEGL operations is GeglOperation.
  GeglOperationClass *operation_class = GEGL_OPERATION_CLASS (klass);

  // The subclass for point filters is GeglOperationPointFilterClass.
  // It specializes, having a process method that is called for each pixel.
  GeglOperationPointFilterClass *point_filter_class = GEGL_OPERATION_POINT_FILTER_CLASS (klass);

  // Override superclasses process methods.
  point_filter_class->process = process;
  operation_class->prepare    = prepare;

  gegl_operation_class_set_keys (operation_class,
                                 "title",       "A Point filter",
                                 "name",        "bootch:my-point-filter",
                                 "blurb",       "A simple point gegl filter",
                                 "version",     "0.1",
                                 "categories",  "Artistic",
                                 "description", "my point gegl filter",
                                 "author",      "Bootch",
                                 NULL);
}

#endif