#ifdef GEGL_PROPERTIES

// Declare properties for this operation.
// This is used by the GEGL properties system to generate a user interface

property_double (blur_amount, "Blur amount", 1.0)
  description   ("Blur amount radius in pixels, to reduce noise before edge detection")
  value_range   (0.0, 10.0)
  ui_meta       ("unit", "pixel-distance")

property_double (weak_threshold, "Weak threshold", 0.3)
  description   ("Threshold to middle gray")
  value_range   (0.0, 1.0)
  ui_meta       ("unit", "pixel-distance")


property_double (strong_threshold, "Strong threshold", 0.8)
  description   ("Threshold to white")
  value_range   (0.0, 1.0)
  ui_meta       ("unit", "pixel-distance")

// property_boolean (should_remove_weak, "Hide weak values", TRUE)
//  description   ("Show middle gray values, or set to black")

#else

// Boilerplate code for a GEGL operation

// Declare is a op of type GEGL_OP_META
// This means it is a meta operation, 
// i.e. constructs a graph of primitive operations having a net effect.
#define GEGL_OP_META
#define GEGL_OP_NAME     canny_op
#define GEGL_OP_C_SOURCE canny-op.c

// Base on the above definitions, gegl-op.h generates code for the operation
#include "gegl-op.h"

// There is no way to include enum definitions private to gegl:image-gradient


/* Return a Gegl node that converts an image to grayscale.
 */
GeglNode *
make_grayscale_node (GeglNode *gegl)
{
  return gegl_node_new_child (gegl, "operation", "gegl:gray", NULL);
}



/* Return a Gegl node that blurs,
 * Hardcoded to kind gaussian, size 5. 
 */
GeglNode *
make_blur_node (
  GeglNode *gegl,
  /* This is the standard deviation in pixels, i.e. how much to blur.
   * The larger the value, the more blurring.
   */
  double blur_amount)
{
  return gegl_node_new_child (gegl,
                              "operation", "gegl:gaussian-blur",
                              "std-dev-x", blur_amount,
                              "std-dev-y", blur_amount,
                              NULL);
}

/* Return a Gegl node that detects edges, in some broad sense.
 * This hides the choice of one of many possible edge-detectors.
 * The choice of edge detection algorithm is made here.
 * This is an inner edge detection node; canny wraps it
 * with other operations to produce a final result.
 * 
 * The usual choice for Canny edge detection is the Sobel operator.
 * 
 * There are many edge detection algorithms available in GEGL 0.4 :case
 * "gegl:edge-sobel" "gegl:edge-laplace" "gegl:edge-neon"
 * 
 * "gegl:edge-sobel" is non-standard (see its implementation in GEGL source code).
 * 
 * "gegl:image-gradient" is the usual Sobel operator.
 * 
 * The usual Sobel operator computes the gradient of image intensity.
 * The gradient in math is a vector field (an array of vectors)
 * The Sobel operator computes a vector (magnitude and direction) at each pixel.
 * The vector points in the direction of the greatest rate of increase of intensity,
 * and its magnitude (length) is the rate of change in that direction.
 * So the result is 2 channels.
 * 
 * The output can be rendered as a grayscale image where edges are highlighted.
 * The rendering is usually of only the magnitude channel, ignoring the direction channel.
 * But other rendering is possible, such as using the direction channel to colorize edges. 
 * 
 * The canny filter uses the direction channel to thin edges.
 * The Sobel operator alone produces thick edges. 
 */
GeglNode *
make_edge_detect_node (GeglNode *gegl)
{
  
  #ifdef EDGE_SOEBEL

  /* edge-sobel is not suited: it does not compute direction. */
  return gegl_node_new_child (gegl, "operation", "gegl:edge-sobel", NULL);

  #else

  /* 
  Note that image-gradient internally converts to format RGB, dropping alpha.
  See the source code for gegl:image-gradient.
  The output is a 2-channel image.
  */
  return gegl_node_new_child (
    gegl, 
    "operation",   "gegl:image-gradient",
    "output-mode", 2, // FAIL: "both", GEGL_IMAGEGRADIENT_BOTH, // both magnitude and direction
    NULL);

  #endif
}

/* Return a Gegl node that thins edges. */
GeglNode *
make_edge_thinning_node (GeglNode *gegl)
{
  return gegl_node_new_child (gegl, "operation", "bootch:non-max-suppression", NULL);
}


/*
See double-threshold-op.c for the definition of this operation.
Keeps middle gray values.
Changes lower values to black 
Changes higher values to white.
*/
GeglNode *
make_threshold_node (GeglNode *gegl)
{
  return gegl_node_new_child (gegl,  "operation", "bootch:double-threshold", NULL);
}

GeglNode *
make_hysteresis_node (GeglNode *gegl)
{
  return gegl_node_new_child (gegl, "operation", "bootch:hysteresis", NULL);
}

/*
Make a node to remove (to black) the weak values.
This hides a choice of implementation.
Here we use double-threshold, since we already used it.
Alternatively, we could use a simple threshold.

Later we set the low and high thresholds to the same value, strong-threshold,
which effectively removes the weak values.
This is a trick to make the double threshold into a single threshold.
*/
GeglNode *
make_weak_remove_node (GeglNode *gegl)
{
  return gegl_node_new_child (gegl,
                              "operation", "bootch:double-threshold",
                              "low-threshold", 0.5, // moot, changed later
                              "high-threshold", 0.5,
                              NULL);
}





/* Create a graph of operations.
 * No code here to construct any specific primitive operations, i.e. node. 
 * They are constructed in separate functions.
 */
static void
attach (GeglOperation *operation)
{
  GeglNode *gegl = operation->node;

  // gboolean should_remove_weak = GEGL_PROPERTIES (operation)->should_remove_weak;

  // Nodes with params redirected to self's params.
  GeglNode *threshold_node = make_threshold_node (gegl);
  GeglNode *blur_node      = make_blur_node (gegl, 3.0);
  GeglNode *final_threshold_node = make_weak_remove_node (gegl);

  /* Call variadic function to link operations,
   * i.e. create a graph that is a sequence i.e. chain.
   * Terminate variadic args with NULL.
   * 
   * Order of operations is important.
   * The output of one operation is the input to the next.
   */
  gegl_node_link_many (
    gegl_node_get_input_proxy  (gegl, "input"),
    
    /* Canny is this sequence of operations. */
    
    // convert to grayscale (to reduce computation, the final result is grayscale)
    make_grayscale_node (gegl),
    // format is now Y' or Y'A float, i.e. channels gray w alpha.

    // blur, to reduce noise
    blur_node,

    // sobel edge detection. Result edges are thick.
    make_edge_detect_node (gegl),
    // format is now float[2], i.e. channels magnitude and direction.
    // Note we have lost any alpha channel, it is not needed for edges.

    // Thin edges, aka non maximum suppression.
    make_edge_thinning_node (gegl),

    // TODO discard direction channel,

    // double threshold magnitude channel
    threshold_node,

    // hysteresis edge tracking
    make_hysteresis_node (gegl),

    final_threshold_node,

    // Image is grayscale
    // We don't convert to indexed color, black and white

    gegl_node_get_output_proxy (gegl, "output"),
    NULL);

  /* Redirect this meta op's properties
   * to the interior node's properties.
   */

  // Same outer param passed for x and y std-dev.
  gegl_operation_meta_redirect (operation, "blur-amount", blur_node, "std-dev-x");
  gegl_operation_meta_redirect (operation, "blur-amount", blur_node, "std-dev-y");
  
  /* Names weak, strong traditional for Canny. */
  gegl_operation_meta_redirect (operation, "weak-threshold",   threshold_node, "low-threshold");
  gegl_operation_meta_redirect (operation, "strong-threshold", threshold_node, "high-threshold");

  /* Redirect the strong-threshold to both params of the final threshold node.
   * This is a trick to make the double-threshold into a single threshold.
   * This removes the weak values.
   */
  gegl_operation_meta_redirect (operation, "strong-threshold", final_threshold_node, "low-threshold");
  gegl_operation_meta_redirect (operation, "strong-threshold", final_threshold_node, "high-threshold");

}


static void
gegl_op_class_init (GeglOpClass *klass)
{
  GeglOperationClass *operation_class = GEGL_OPERATION_CLASS (klass);

  // Override superclasses attach method.
  operation_class->attach = attach;

  gegl_operation_class_set_keys (operation_class,
                                 "title",       "Canny edge detect filter",
                                 "name",        "bootch:canny",
                                 "blurb",       "Generate b/w, thinned edges from an image",
                                 "version",     "0.1",
                                 "categories",  "edge-detect",
                                 "description", "Canny filter",
                                 "author",      "Bootch",
                                 NULL);
}

#endif