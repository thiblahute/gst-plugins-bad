schro_sources = [
  'gstschro.c',
  'gstschrodec.c',
  'gstschroenc.c',
  'gstschroutils.c',
]

schro_comp = component('schro')
schro_dep = dependency('schroedinger-1.0', version : '>= 1.0.10', required : false, components: schro_comp)

gstschro = library('gstschro', schro_sources,
  c_args : gst_plugins_bad_args,
  link_args : noseh_link_args,
  include_directories : [configinc],
  dependencies : [gstvideo_dep, schro_comp.dependencies()],
  install : true,
  install_dir : plugins_install_dir,
  components: schro_comp
)
