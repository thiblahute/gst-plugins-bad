gsm_comp = component('gsm')
gsm_dep = cc.find_dependency('gsm',
  libraries: ['gsm'],
  headers_symbols: ['gsm.h', 'gsm_create'],
  required : false,
  components: gsm_comp)

gstgsm = library('gstgsm',
  ['gstgsm.c', 'gstgsmenc.c', 'gstgsmdec.c'],
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstaudio_dep, gsm_comp.dependencies()],
  install : true,
  install_dir : plugins_install_dir,
  components: gsm_comp
)
