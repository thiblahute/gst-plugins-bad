faad_comp = component('faad')
faad_dep = cc.find_dependency('faad',
  headers_symbols: [['neaacdec.h', 'NeAACDecOpen'], ['neaacdec.h', 'LATM']],
  libs: 'faad',
  required: false,
  components: faad_comp
)

gstfaad = library('gstfaad',
  'gstfaad.c',
  c_args : gst_plugins_bad_args + [ '-DFAAD2_MINOR_VERSION=7', '-DFAAD_IS_NEAAC' ],
  link_args : noseh_link_args,
  include_directories : [configinc],
  dependencies : [gstaudio_dep, gstpbutils_dep, gsttag_dep, faad_dep],
  install : true,
  install_dir : plugins_install_dir,
  components: faad_comp
)
