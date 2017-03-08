x265_comp = component('x265')
x265_dep = dependency('x265', required: false,
  components: x265_comp)

gstx265 = library('gstx265',
  'gstx265enc.c',
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstpbutils_dep, gstvideo_dep, x265_comp.dependencies()],
  install : true,
  install_dir : plugins_install_dir,
  components: x265_comp
)
