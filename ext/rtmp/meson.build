rtmp_sources = [
  'gstrtmp.c',
  'gstrtmpsink.c',
  'gstrtmpsrc.c',
]

rtmp_comp = component('rtmp')
rtmp_dep = dependency('librtmp', required : false, components: rtmp_comp)

gstrtmp = library('gstrtmp',
  rtmp_sources,
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstbase_dep, rtmp_comp.dependencies()] + winsock2,
  install : true,
  install_dir : plugins_install_dir,
  components: rtmp_comp
)
