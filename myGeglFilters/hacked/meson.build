
mathDep = meson.get_compiler('c').find_library('m', required: false)

shared_library('hacked-image-gradient',
               ['image-gradient.c', ],
               dependencies : [geglDependency, mathDep],
               name_prefix : '',
               install: true,
               install_dir: userInstallPath,
               )