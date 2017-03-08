ass_comp = component('assrender')
ass_dep = dependency('libass', version : '>= 0.10.2', required : false,
  components: ass_comp)

gstassrender = library('gstassrender',
  'gstassrender.c',
  c_args : gst_plugins_bad_args,
  link_args : noseh_link_args,
  include_directories : [configinc],
  dependencies : [gstvideo_dep, ass_comp.dependencies()],
  install : true,
  install_dir : plugins_install_dir,
  components: ass_comp
)
