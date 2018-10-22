badvideo_sources = [
  'gstvideoaggregator.c',
]
badvideo_headers = [
  'gstvideoaggregator.h',
  'video-bad-prelude.h',
]
install_headers(badvideo_headers, subdir : 'gstreamer-1.0/gst/video')


gstbadvideo = library('gstbadvideo-' + api_version,
  badvideo_sources,
  c_args : gst_plugins_bad_args + ['-DGST_USE_UNSTABLE_API', '-DBUILDING_GST_VIDEO_BAD'],
  include_directories : [configinc, libsinc],
  version : libversion,
  soversion : soversion,
  darwin_versions : osxversion,
  install : true,
  dependencies : [gstvideo_dep, gstbase_dep],
)

if build_gir
  badvideo_gir = gnome.generate_gir(gstbadvideo,
    sources : badvideo_sources + badvideo_headers,
    namespace : 'GstBadVideo',
    nsversion : api_version,
    identifier_prefix : 'Gst',
    symbol_prefix : 'gst',
    export_packages : 'gstreamer-bad-video-1.0',
    includes : ['Gst-1.0'],
    extra_args : gir_init_section + ['-DGST_USE_UNSTABLE_API'],
    dependencies : [gst_dep],
    install : false
  )
endif

gstbadvideo_dep = declare_dependency(link_with : gstbadvideo,
  include_directories : [libsinc],
  dependencies : [gstvideo_dep, gstbase_dep])
