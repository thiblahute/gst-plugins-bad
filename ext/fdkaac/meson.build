fdkaac_comp = component('fdkaac')
fdkaac_dep = dependency('fdk-aac', required : false,
  components: fdkaac_comp)

gstfdkaac = library('gstfdkaac',
  ['plugin.c', 'gstfdkaacenc.c', 'gstfdkaacdec.c'],
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstaudio_dep, gstpbutils_dep, fdkaac_comp.dependencies()],
  install : true,
  install_dir : plugins_install_dir,
  components: fdkaac_comp
)
