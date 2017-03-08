dtsdec_comp = component('dtsdec')

if not dependency('libdca', required : false).found()
  dca_dep = cc.find_dependency('dca',
    libs: ['dca'],
    headers_symbols: [['dca.h', 'dca_init']],
    required : false,
    components: dtsdec_comp
  )
else
  dca_dep = dependency('libdca', required : false,
    components: dtsdec_comp)
endif

gstdtsdec = library('gstdtsdec',
  'gstdtsdec.c',
  c_args : gst_plugins_bad_args,
  link_args : noseh_link_args,
  include_directories : [configinc, libsinc],
  dependencies : [gstaudio_dep, orc_dep, dca_dep],
  install : true,
  install_dir : plugins_install_dir,
  components: dtsdec_comp
)
