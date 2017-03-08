faac_comp = component('faac')
faac_dep = cc.find_dependency('faac',
  libs: ['faac'],
  headers_symbols: [['faac.h', 'faacEncOpen']],
  components: faac_comp,
  required: false
)

gstfaac = library('gstfaac',
  'gstfaac.c',
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstaudio_dep, gstpbutils_dep, gsttag_dep, faac_comp.dependencies()],
  install : true,
  install_dir : plugins_install_dir,
  components: faac_comp
)
