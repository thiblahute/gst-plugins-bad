resindvd_sources = [
  'gstmpegdemux.c',
  'gstmpegdesc.c',
  'gstpesfilter.c',
  'plugin.c',
  'resindvdbin.c',
  'resindvdsrc.c',
  'rsndec.c',
  'rsninputselector.c',
  'rsnparsetter.c',
]

resindvd_comp = component('resindvd')
dependency('dvdnav', version : '>= 4.1.2', required : false, components: resindvd_comp)
dependency('dvdread', version : '>= 4.1.2', required : false, components: resindvd_comp)

gstresindvd = library('gstresindvd', resindvd_sources,
  c_args : gst_plugins_bad_args,
  link_args : noseh_link_args,
  include_directories : [configinc, libsinc],
  dependencies : [gstvideo_dep, gstpbutils_dep, gsttag_dep,
                  resindvd_comp.dependencies(), gmodule_dep],
  install : true,
  install_dir : plugins_install_dir,
  components: resindvd_comp
)
