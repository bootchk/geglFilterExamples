project('my-graph-filter', 'c', version : '0.1')

geglDependency = dependency('gegl-0.4', required : true)

# Install to a user-specific location where gegl looks.

# This is default WHEN $XDG_DATA_HOME is not set!
userInstallPath = '/work/.home/.local/share/gegl-0.4/plug-ins/'

# A gegl filter is-a shared library.
# But we don't want to install where most libraries are.
# We want to install to $XDG_DATA_HOME, where gegl looks.

# Example from tutorial at gimp dev site
subdir('metaOp')

# My examples
subdir('graphOp')
subdir('pointOp')
subdir('areaOp')

# Ops used by canny
subdir('nonMaxSuppressionOp')

# Canny edge detector
subdir('cannyOp')