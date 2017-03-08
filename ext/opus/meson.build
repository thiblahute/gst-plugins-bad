opus_sources = [
  'gstopus.c',
  'gstopusheader.c',
  'gstopusparse.c',
]

opus_comp = component('opus')
opus_dep = dependency('opus', version : '>= 0.9.4', required : false,
  components: opus_comp)

gstopus = library('gstopusparse',
  opus_sources,
  c_args : gst_plugins_bad_args,
  link_args : noseh_link_args,
  include_directories : [configinc],
  dependencies : [gstrtp_dep, gstpbutils_dep, gstaudio_dep,
                  gsttag_dep, opus_comp.dependencies(), libm],
  install : true,
  install_dir : plugins_install_dir,
  components: opus_comp
)
