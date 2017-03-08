sbc_sources = [
  'sbc-plugin.c',
  'gstsbcdec.c',
  'gstsbcenc.c',
]

sbc_comp = component('sbc')
sbc_dep = dependency('sbc', version : '>= 1.0', required : false, components: sbc_comp)

gstsbc = library('gstsbc',
  sbc_sources,
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstaudio_dep, sbc_comp.dependencies()],
  install : true,
  install_dir : plugins_install_dir,
  components: sbc_comp
)
