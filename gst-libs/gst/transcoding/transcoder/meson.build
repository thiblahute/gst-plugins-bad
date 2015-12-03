# The GstTranscoder library
install_headers('gsttranscoder.h', subdir : 'gstreamer-1.0/gst/transcoding/transcoder')

gst_transcoder = shared_library('gsttranscoder',
    'gsttranscoder.c',
    install: true,
    include_directories : [configinc, libsinc],
    dependencies: [glib_dep, gst_dep, gstpbutils_dep],
    c_args: ['-Wno-pedantic'])

gen_sources = []
if build_gir
  transcoder_gir = gnome.generate_gir(gst_transcoder,
    sources : ['gsttranscoder.h', 'gsttranscoder.c'],
    nsversion : api_version,
    namespace : 'GstTranscoder',
      identifier_prefix : 'Gst',
      symbol_prefix : 'gst_',
    includes : ['GObject-2.0', 'Gst-' + api_version,
        'GstPbutils-' + api_version],
    dependencies: [gst_dep],
    install : true,
    extra_args : gir_init_section + ['-DGST_USE_UNSTABLE_API'],
  )
  gen_sources += transcoder_gir
endif

gsttranscoder_dep = declare_dependency(link_with : gst_transcoder,
  include_directories : [libsinc],
  sources: gen_sources,
  dependencies : [glib_dep, gst_dep, gstpbutils_dep])
