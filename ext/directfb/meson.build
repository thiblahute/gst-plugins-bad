directfb_comp = component('directfb')
directfb_dep = dependency('directfb', version : '>= 0.9.24', required : false,
  components: directfb_comp)

gstdirectfb = library('gstdirectfb',
  'dfbvideosink.c',
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstvideo_dep, directfb_comp.dependencies()],
  install : true,
  install_dir : plugins_install_dir,
  components: directfb_comp
)
