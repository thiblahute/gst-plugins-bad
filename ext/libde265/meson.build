de265_sources = [
  'gstlibde265.c',
  'libde265-dec.c',
]

de265_comp = component('de265')
de265_dep = dependency('libde265', version : '>= 0.9', required : false,
  components: de265_comp)

gstde265 = library('gstde265',
  de265_sources,
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstvideo_dep, de265_comp.dependencies()],
  install : true,
  install_dir : plugins_install_dir,
  components: de265_comp,
)
