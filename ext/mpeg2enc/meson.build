mpeg2enc_comp = component('mpeg2enc', dependencies: mjpegtools_dep)
mpeg2enc_dep = cxx.find_dependency('mpeg2encpp',
  libs: 'mpeg2enc',
  required : false,
  components: mpeg2enc_comp)

gstmpeg2enc = library('gstmpeg2enc',
  'gstmpeg2enc.cc',
  'gstmpeg2encoptions.cc',
  'gstmpeg2encoder.cc',
  'gstmpeg2encstreamwriter.cc',
  'gstmpeg2encpicturereader.cc',
  cpp_args : gst_plugins_bad_args + ['-DGST_MJPEGTOOLS_API=' + mjpegtools_api],
  link_args : noseh_link_args,
  include_directories : [configinc, libsinc],
  dependencies : [gstvideo_dep, mpeg2enc_comp.dependencies()],
  install : true,
  install_dir : plugins_install_dir,
  components: mpeg2enc_comp
)
