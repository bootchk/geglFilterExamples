mathDep = meson.get_compiler('c').find_library('m', required: false)

shared_library('false-color-filter',
               'false-color-filter.c',
               dependencies : [geglDependency, mathDep],
               name_prefix : '',
               install: true,
               install_dir: userInstallPath,
               )