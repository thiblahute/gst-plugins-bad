curl_sources = [
  'gstcurlbasesink.c',
  'gstcurl.c',
  'gstcurlfilesink.c',
  'gstcurlftpsink.c',
  'gstcurlhttpsink.c',
  'gstcurlsftpsink.c',
  'gstcurlsmtpsink.c',
  'gstcurlsshsink.c',
  'gstcurltlssink.c',
  'gstcurlhttpsrc.c',
  'gstcurlqueue.c',
]

curl_comp = component('curl')
curl_dep = dependency('libcurl', version : '>= 7.35.0', required : false,
  components: curl_comp)

gstcurl = library('gstcurl',
  curl_sources,
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstbase_dep, curl_comp.dependencies()] + winsock2,
  install : true,
  install_dir : plugins_install_dir,
  components: curl_comp
)
