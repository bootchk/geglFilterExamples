#ifdef GEGL_PROPERTIES

  /* none yet */

#else

// Boilerplate code for a GEGL operation

// Declare is a op of type GEGL_OP_META
// This means it is a meta operation, i.e. it does not process data directly,
// but rather constructs a graph of operations that do the actual processing.
#define GEGL_OP_META
#define GEGL_OP_NAME     my_graph_op
#define GEGL_OP_C_SOURCE my-graph-op.c

// Base on the above definitions, gegl-op.h generates code for the operation
#include "gegl-op.h"


/* Create a graph of operations.
 * No code here to construct any specific operations, i.e. node. 
 * They are constructed in separate functions.
 */
static void
attach (GeglOperation *operation)
{
  GeglNode *gegl = operation->node;

  /* Call variadic function to link operations,
   * i.e. create a graph that is a sequence i.e. chain.
   * Terminate variadic args with NULL.
   * 
   * Order of operations is important.
   */
  gegl_node_link_many (
    gegl_node_get_input_proxy  (gegl, "input"),

    /* 
    Typically, you have more nodes here.
    Primitive operations that do the actual processing.
    */
    // my_make_grayscale_node (gegl),
    
    gegl_node_get_output_proxy (gegl, "output"),
    NULL);
}


static void
gegl_op_class_init (GeglOpClass *klass)
{
  GeglOperationClass *operation_class = GEGL_OPERATION_CLASS (klass);

  // Override superclasses attach method.
  operation_class->attach = attach;

  gegl_operation_class_set_keys (operation_class,
                                 "title",       "A Graph/Meta filter",
                                 "name",        "bootch:my-graph-filter",
                                 "blurb",       "A simple graph gegl filter",
                                 "version",     "0.1",
                                 "categories",  "Artistic",
                                 "description", "my graph gegl filter",
                                 "author",      "Bootch",
                                 NULL);
}

#endif