#ifdef GEGL_PROPERTIES

property_double (magnitude_emphasis, "Emphasis", 2.0)
    value_range (1.0, 10.0)
    ui_range    (1, 10)
    description("Emphasize the magnitude of the gradient by this factor.")

#else

// Boilerplate code for a GEGL operation

// Declare is a op of type GEGL_OP_POINT_FILTER
// This is a point operation, i.e. it processes each pixel independently.
#define GEGL_OP_POINT_FILTER
#define GEGL_OP_NAME     false_color_op
#define GEGL_OP_C_SOURCE false-color-filter.c

// Base on the above definitions, gegl-op.h generates code for the operation
#include "gegl-op.h"

#include <math.h>

static void prepare (GeglOperation *operation)
{
  const Babl *space = gegl_operation_get_source_space (operation, "input");

   const Babl *gradient_format= babl_format_n (babl_type ("float"), 2);

  // Set the input format to a two-channel float format, which is suitable for gradients.
  gegl_operation_set_format (operation, "input",  gradient_format);
  // gegl_operation_set_format (operation, "output", babl_format_with_space ("R'G'B'A float", space));
  gegl_operation_set_format (operation, "output", babl_format_with_space ("HSV float", space));
}



#define input_FPP  2 // Floats per pixel for the input format
#define output_FPP 3 // Floats per pixel for the output format (HSV float has three channels)




/*
Convert a vector field gradient to false colors.

Direction of the gradient => Hue in HSV color space.

Convert the gradient direction/angle to a hue value.
The hue is calculated as the angle of the gradient vector, normalized to [0, 1].
The in angle is in radians in range [-PI, PI]
add PI to shift it to [0, 2*PI], and then divide by 2*PI to normalize it to [0, 1].
      
The out hue is in the range [0, 1], where 0 corresponds to red, 0.33 to green, and 0.66 to blue.
      
The gradient direction is given by the second channel of the input buffer.
The hue is stored in the first channel of the output buffer.

The magnitude of the gradient => Value and Saturation in HSV color space.

This is only one way to visualize a vector field.
Another way is to visualize the gradient magnitude as brightness (grayscale)
and not represent the direction at all, or as transparency.
(Which is what gegl:image-gradient does, it shows a grayscale image i.e. Y'A)

An alternative implementation might be in BABL.
*/ 
static gboolean
process (GeglOperation       *op,
         void                *in_buf,
         void                *out_buf,
         glong                n_pixels,
         const GeglRectangle *roi,
         gint                 level)
{
  // Pointers used to scan buffer.
  gfloat *in  = in_buf;
  gfloat *out = out_buf;
  
  gfloat magnitude_emphasis  = GEGL_PROPERTIES (op)->magnitude_emphasis;

  for (glong i=0; i<n_pixels; i++)
    {
      
      float calculated_hue = (in[1] + M_PI) / (2 * M_PI);
      float calculated_magnitude = in[0];

      out[0] = calculated_hue;

      // Amplify/emphasize low values of the gradient magnitude.
      // Get the nth root of the magnitude to emphasize low values.
      calculated_magnitude = pow(calculated_magnitude, 1.0 / magnitude_emphasis);

      // Magnitude of the gradient => Value and Saturation in HSV color space.
      out[2] = calculated_magnitude;
      out[1] = calculated_magnitude;

      // Alternatively, Constant, full Saturation in HSV color space.
      // out[1] = 1.0f; // Full saturation

      // Move to the next pixel.
      in  += input_FPP;
      out += output_FPP;
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
                                 "title",       "False color a vector field",
                                 "name",        "bootchk:false-color-filter",
                                 "blurb",       "TODO",
                                 "version",     "0.1",
                                 "categories",  "visualization",
                                 "description", "Visualize a vector field in false colors.",
                                 "author",      "lloyd konneker",
                                 NULL);
}

#endif