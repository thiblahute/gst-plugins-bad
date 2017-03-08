spandsp_sources = [
  'gstdtmfdetect.c',
  'gstdtmfdetect.h',
  'gstspandsp.c',
  'gstspanplc.c',
  'gstspanplc.h',
  'gsttonegeneratesrc.c',
  'gsttonegeneratesrc.h',
]

spandsp_comp = component('spandsp')
spandsp_dep = dependency('spandsp', version : '>= 0.0.6', required : false,
  components: spandsp_comp)

gstspandsp = library('gstspandsp',
  spandsp_sources,
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstvideo_dep, spandsp_comp.dependencies()],
  install : true,
  install_dir : plugins_install_dir,
  components: spandsp_comp
)
