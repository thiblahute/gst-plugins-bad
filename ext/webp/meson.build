webp_sources = [
  'gstwebp.c',
  'gstwebpdec.c',
  'gstwebpenc.c',
]

webp_comp = component('webp')
webp_dep = dependency('libwebp', version : '>= 0.2.1', required : false,
  components: webp_comp)

gstwebp = library('gstwebp',
  webp_sources,
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstvideo_dep, webp_comp.dependencies()],
  install : true,
  install_dir : plugins_install_dir,
  components: webp_comp
)
