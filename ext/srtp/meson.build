srtp_sources = [
  'gstsrtp.c',
  'gstsrtpdec.c',
  'gstsrtpenc.c',
]

srtp_comp = component('srtp')
srtp_dep = dependency('libsrtp', required : false,
  components: srtp_comp)
if not srtp_dep.found()
  srtp_dep = cc.find_dependency('srtp', libs: 'srtp',
    headers_symbols: ['srtp/srtp.h', 'srtp_init'],
    required : false, components: srtp_comp)
endif

mkenums = find_program('srtp_mkenum.py')
gstsrtp_h = custom_target('gstsrtpenum_h',
  output : 'gstsrtp-enumtypes.h',
  input : 'gstsrtp.h',
  command : [mkenums, glib_mkenums, '@OUTPUT@', '@INPUT@'])

gstsrtp_c = custom_target('gstsrtpenum_c',
  output : 'gstsrtp-enumtypes.c',
  input : 'gstsrtp.h',
  depends : [gstsrtp_h],
  command : [mkenums, glib_mkenums, '@OUTPUT@', '@INPUT@'])

gstsrtp = library('gstsrtp',
  srtp_sources, gstsrtp_c, gstsrtp_h,
  c_args : gst_plugins_bad_args,
  link_args : noseh_link_args,
  include_directories : [configinc],
  dependencies : [gstrtp_dep, gstvideo_dep, srtp_comp.dependencies()],
  install : true,
  install_dir : plugins_install_dir,
  components: srtp_comp
)
