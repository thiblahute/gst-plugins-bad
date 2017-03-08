gstopencv_sources = [
  'gstcvdilate.cpp',
  'gstcvdilateerode.cpp',
  'gstcvequalizehist.cpp',
  'gstcverode.cpp',
  'gstcvlaplace.cpp',
  'gstcvsmooth.cpp',
  'gstcvsobel.cpp',
  'gstdisparity.cpp',
  'gstedgedetect.cpp',
  'gstfaceblur.cpp',
  'gstfacedetect.cpp',
  'gstgrabcut.cpp',
  'gsthanddetect.cpp',
  'gstmotioncells.cpp',
  'gstopencv.cpp',
  'gstretinex.cpp',
  'gstsegmentation.cpp',
  'gstskindetect.cpp',
  'gsttemplatematch.cpp',
  'gsttextoverlay.cpp',
  'MotionCells.cpp',
  'motioncells_wrapper.cpp',
  'gstdewarp.cpp'
]

libopencv2_headers = [
  'opencv2/core/core_c.h',
  'opencv2/core/version.hpp',
  'opencv2/highgui/highgui_c.h',
  'opencv2/imgproc/imgproc.hpp',
  'opencv2/imgproc/imgproc_c.h',
  'opencv2/objdetect/objdetect.hpp',
  'opencv2/video/background_segm.hpp',
]

libopencv3_headers = [
  'opencv2/bgsegm.hpp',
]

gstopencv_cargs = ['-DGST_HAAR_CASCADES_DIR="@0@"']
opencv_comp = component('opencv')
opencv_dep = dependency('opencv', version : ['>= 2.3.0', '<= 3.2.0'], required : false,
  components: opencv_comp)

if opencv_dep.found()
  cxx.find_dependency('opencv2',
      headers: libopencv2_headers,
      components: opencv_comp)

  if opencv_dep.version() >= '3.0.0'
    cxx.find_dependency('opencv3',
        headers: ['opencv2/core/core_c.h', 'opencv2/core/version.hpp',
          'opencv2/highgui/highgui_c.h', 'opencv2/imgproc/imgproc.hpp',
          'opencv2/imgproc/imgproc_c.h', 'opencv2/objdetect/objdetect.hpp',
          'opencv2/video/background_segm.hpp',],
        components: opencv_comp)
  endif
endif

if opencv_comp.enabled()
  opencv_prefix = opencv_dep.get_pkgconfig_variable('prefix')
  gstopencv_cargs += ['-DOPENCV_PREFIX="' + opencv_prefix + '"']

  # Check the data dir used by opencv for its xml data files
  # Use prefix from pkg-config to be compatible with cross-compilation
  r = run_command('test', '-d', opencv_prefix + '/share/opencv')
  if r.returncode() == 0
    gstopencv_cargs += '-DOPENCV_PATH_NAME="opencv"'
  else
    r = run_command('test', '-d', opencv_prefix + '/share/OpenCV')
    if r.returncode() == 0
      gstopencv_cargs += '-DOPENCV_PATH_NAME="OpenCv"'
    else
      error('Unable to detect OpenCV data directory')
    endif
  endif
  gstopencv = library('gstopencv',
    gstopencv_sources,
    cpp_args : gst_plugins_bad_args + gstopencv_cargs + [ '-DGST_USE_UNSTABLE_API' ],
    link_args : noseh_link_args,
    include_directories : [configinc],
    dependencies : [gstbase_dep, gstvideo_dep, opencv_comp.dependencies(), gstopencv_dep],
    install : true,
    install_dir : plugins_install_dir,
    components: opencv_comp
  )
endif
