rsvg_sources = [
  'gstrsvg.c',
  'gstrsvgdec.c',
  'gstrsvgoverlay.c',
]

rsvg_comp = component('rsgv')
rsvg_dep = dependency('librsvg-2.0', version : '>= 2.36.2', required : false,
  components: rsvg_comp)

gstrsvg = library('gstrsvg',
  rsvg_sources,
  c_args : gst_plugins_bad_args,
  link_args : noseh_link_args,
  include_directories : [configinc],
  dependencies : [gstvideo_dep, rsvg_comp.dependencies()],
  install : true,
  install_dir : plugins_install_dir,
  components: rsvg_comp
)
