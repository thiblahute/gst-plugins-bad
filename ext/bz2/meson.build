bz2_sources = [
  'gstbz2.c',
  'gstbz2dec.c',
  'gstbz2enc.c',
]

bz2_comp = component('bz2')

bz2_dep = cc.find_dependency('bz2',
  libs: ['bz2'],
  headers_symbols: [['bzlib.h', 'BZ2_bzlibVersion']],
  required : false,
  components: bz2_comp)

gstbz2 = library('gstbz2',
  bz2_sources,
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstvideo_dep, bz2_comp.dependencies()],
  install : true,
  install_dir : plugins_install_dir,
  components: bz2_comp
)
