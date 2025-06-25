#ifdef GEGL_PROPERTIES

property_double (magnitude_emphasis, "Emphasis", 2.0)
    value_range (1.0, 10.0)
    ui_range    (1, 10)
    description("Emphasize the magnitude of the gradient by this factor.")

#else

// Boilerplate code for a GEGL operation

// Declare is a op of type GEGL_OP_META
// This means it is a meta operation, i.e. it does not process data directly,
// but rather constructs a graph of operations that do the actual processing.
#define GEGL_OP_META
#define GEGL_OP_NAME     false_color_gradient_op
#define GEGL_OP_C_SOURCE false-color-gradient-filter.c

// Base on the above definitions, gegl-op.h generates code for the operation
#include "gegl-op.h"


/* Create a graph of operations. */
static void
attach (GeglOperation *operation)
{
  GeglNode *gegl = operation->node;
  GeglNode *false_color_gradient_node;

  gfloat magnitude_emphasis  = GEGL_PROPERTIES (operation)->magnitude_emphasis;

  /* Call variadic function to link operations,
   * i.e. create a graph that is a sequence i.e. chain.
   * Terminate variadic args with NULL.
   * 
   * Order of operations is important.
   */
  gegl_node_link_many (
    gegl_node_get_input_proxy  (gegl, "input"),

    /* Primitive operations that do the actual processing. */

    // Calculate gradient.
    gegl_node_new_child (
      gegl, 
      "operation",   "gegl:image-gradient",
      "output-mode", 2, // GEGL_IMAGEGRADIENT_BOTH, both magnitude and direction
      NULL),
    
    // Format of the gradient is now float[2], i.e. channels magnitude and direction.
    // Magnitude in range [0, 1], direction in range [-PI, PI].
     
    // Convert gradient to false colors.
    false_color_gradient_node =
    gegl_node_new_child (
      gegl, 
      "operation",   "bootchk:false-color-filter",
      "magnitude_emphasis", magnitude_emphasis,
      NULL),

    gegl_node_get_output_proxy (gegl, "output"),
    NULL);

  /* Redirect this meta op's properties to internal nodes. */
  gegl_operation_meta_redirect (operation,                 "magnitude-emphasis", 
                                false_color_gradient_node, "magnitude-emphasis");
}


static void
gegl_op_class_init (GeglOpClass *klass)
{
  GeglOperationClass *operation_class = GEGL_OPERATION_CLASS (klass);

  // Override superclasses attach method.
  operation_class->attach = attach;

  gegl_operation_class_set_keys (operation_class,
                                 "title",       "False color the gradient field",
                                 "name",        "bootchk:false-color-gradient-filter",
                                 "blurb",       "",
                                 "version",     "0.1",
                                 "categories",  "visualization",
                                 "description", "Shows image's gradient in false colors."
                                                "A peak appears like an RGB color wheel, green to upper left.",
                                 "author",      "lloyd konneker",
                                 NULL);
}

#endif