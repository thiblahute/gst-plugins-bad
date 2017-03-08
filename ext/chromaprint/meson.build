chromaprint_comp = component('chromaprint')
chromaprint_dep = dependency('libchromaprint', required : false,
  components: chromaprint_comp)

gstchromaprint = library('gstchromaprint',
  'gstchromaprint.c',
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstaudio_dep, chromaprint_comp.dependencies()],
  install : true,
  install_dir : plugins_install_dir,
  components: chromaprint_comp,
)
