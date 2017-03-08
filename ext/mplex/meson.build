mplex2_comp = component('mplex', dependencies: mjpegtools_dep)
mplex2_dep = cxx.find_dependency('mplex2', libs: 'mplex2',
  required : false, components: mplex2_comp)

gstmplex2 = library('gstmplex',
  'gstmplex.cc',
  'gstmplexibitstream.cc',
  'gstmplexjob.cc',
  'gstmplexoutputstream.cc',
  cpp_args : gst_plugins_bad_args + ['-DGST_MJPEGTOOLS_API=' + mjpegtools_api],
  link_args : noseh_link_args,
  include_directories : [configinc, libsinc],
  dependencies : [gstaudio_dep, mplex2_comp.dependencies()],
  install : true,
  install_dir : plugins_install_dir,
  components: mplex2_comp
)
