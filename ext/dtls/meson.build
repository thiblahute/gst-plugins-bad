dtls_sources = [
  'gstdtlsagent.c',
  'gstdtlscertificate.c',
  'gstdtlsconnection.c',
  'gstdtlsdec.c',
  'gstdtlsenc.c',
  'gstdtlssrtpbin.c',
  'gstdtlssrtpdec.c',
  'gstdtlssrtpdemux.c',
  'gstdtlssrtpenc.c',
  'plugin.c',
]

dtls_comp = component('dtls', dependencies: openssl_dep)
dtls_dep = dependency('libcrypto', required : false,
    components: dtls_comp)

gstdtls = library('gstdtls',
  dtls_sources,
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gst_dep, dtls_comp.dependencies()],
  install : true,
  install_dir : plugins_install_dir,
  components: dtls_comp
)
