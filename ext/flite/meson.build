flite_sources = [
  'gstflite.c',
  'gstflitetestsrc.c',
]

flite_comp = component('flite')
flite_dep = cc.find_dependency('flite',
  libs: [ 'flite', 'flite_cmu_us_kal', 'flite_usenglish', 'flite_cmulex'],
  headers_symbols: [['flite/flite.h', 'flite_init']],
  required : false,
  components: flite_comp
)

gstflite = library('gstflite',
  flite_sources,
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstaudio_dep] + flite_comp.dependencies(),
  install : true,
  install_dir : plugins_install_dir,
  components: flite_comp
)
