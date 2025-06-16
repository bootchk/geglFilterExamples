#ifdef GEGL_PROPERTIES

// Declare properties for this operation.
// This is used by the GEGL properties system to generate a user interface

property_double (blur_amount, "Blur amount", 3.0)
  description   ("Blur amount in pixels")
  value_range   (0.0, 10.0)
  ui_meta       ("unit", "pixel-distance")

property_double (low, "Low", 0.3)
  description   ("Low threshold")
  value_range   (0.0, 0.5)
  ui_meta       ("unit", "pixel-distance")


property_double (high, "High", 0.8)
  description   ("High threshold")
  value_range   (0.5, 1.0)
  ui_meta       ("unit", "pixel-distance")

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
 * 
 * The usual choice for Canny edge detection is the Sobel operator.
 * 
 * There are many edge detection algorithms available in GEGL.
 * gegl:edge-canny does not already exist in GEGL 0.4.
 * "gegl:edge-sobel" "gegl:edge-laplace" "gegl:edge-neon"
 * 
 * "gegl:edge-sobel" is non-standard (see its implementation in GEGL source code).
 * "gegl:image-gradient" is the usual Sobel operator.
 * 
 * The usual Sobel operator computes the gradient of the image intensity at each pixel,
 * highlighting regions of high spatial frequency that correspond to edges.
 * The output is a grayscale image where edges are highlighted.
 * The Sobel operator is often used as a preprocessing step for more complex edge detection algorithms.
 * It is particularly effective for detecting edges in images with well-defined structures,
 * such as lines and shapes, and is less sensitive to noise compared to other edge detection methods.
 * The Sobel operator is a discrete differentiation operator,
 * that computes an approximation of the gradient of the image intensity function.
 * The Sobel operator works by convolving the image with two 3x3 kernels,
 * one for detecting horizontal edges and one for vertical edges.
 * Then combines the two results to get the gradient magnitude AND direction.
 * 
 * The gradient in math is a vector field (an array of vectors)
 * The Sobel operator computes a vector (magnitude and direction) at each pixel.
 */
GeglNode *
make_edge_detect_node (GeglNode *gegl)
{
  // return gegl_node_new_child (gegl, "operation", "gegl:edge-sobel", NULL);
  return gegl_node_new_child (gegl, "operation", "gegl:image-gradient", NULL);
}

GeglNode *
make_edge_thinning_node (GeglNode *gegl)
{
  /* Return a Gegl node that thins edges. */
  return gegl_node_new_child (gegl, "operation", "bootch:non-max-suppression", NULL);
}


/* Threshold with this transfer function:
      __
     |
    /
  _|

 * I.E. middle gray values are kept, 
 * lower values dropped to black, 
 * higher values raised to white.
 */
GeglNode *
make_threshold_node (GeglNode *gegl)
{
  return gegl_node_new_child (gegl, 
                              "operation", "bootch:my-point-filter",
                              // magnitude below this value is set to 0 i.e. black.
                              "low-threshold",  0.33,
                              // magnitude above this value is set to 1 i.e. white.
                              "high-threshold", 0.8,
                              NULL);
}


/* gegl:convert-format */





/* Create a graph of operations.
 * No code here to construct any specific primitive operations, i.e. node. 
 * They are constructed in separate functions.
 */
static void
attach (GeglOperation *operation)
{
  GeglNode *gegl = operation->node;

  GeglNode *threshold_node = make_threshold_node (gegl);
  GeglNode *blur_node      = make_blur_node (gegl, 3.0);


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
    
    // convert to grayscale
    make_grayscale_node (gegl),
    // blur 
    blur_node,
    // sobel edge detection
    make_edge_detect_node (gegl),
    // non maximum suppression
    make_edge_thinning_node (gegl),
    // hysteresis thresholding
    threshold_node,

    gegl_node_get_output_proxy (gegl, "output"),
    NULL);

  /* Redirect this meta op's properties
   * to the interior node's properties.
   */

  gegl_operation_meta_redirect (operation, "blur-amount", blur_node, "std-dev-x");
  gegl_operation_meta_redirect (operation, "blur-amount", blur_node, "std-dev-y");
  

  gegl_operation_meta_redirect (operation, "low", threshold_node, "low-threshold");
  gegl_operation_meta_redirect (operation, "high", threshold_node, "high-threshold");
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