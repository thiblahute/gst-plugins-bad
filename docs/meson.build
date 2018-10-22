html_theme = 'https://github.com/hotdoc/hotdoc_lumen_theme/releases/download/0.6/hotdoc_lumen_theme-0.6.tar.xz?sha256=0e2f175f4cf8c00ed7ac5014e30c806a294b0d3818565eb74e1424a948e8a452'
build_hotdoc = false

hotdoc_extension_dir = []
required_hotdoc_extensions = ['gi-extension', 'c-extension']
if gst_dep.type_name() == 'internal'
    gst_proj = subproject('gstreamer')
    plugins_cache_generator = gst_proj.get_variable('plugins_cache_generator')
    if gst_proj.get_variable('build_hotdoc')
        hotdoc_extension_dir = [gst_proj.get_variable('hotdoc_extension_dir')]
    else
        required_hotdoc_extensions += ['gst-extension']
    endif
else
    required_hotdoc_extensions += ['gst-extension']
    plugins_cache_generator = find_program(join_paths(gst_dep.get_pkg_variable('libexecdir'), 'gstreamer-' + api_version, 'gst-plugins-doc-cache-generator'),
        required: false)
endif

plugins_cache = join_paths(meson.current_source_dir(), 'plugins', 'gst_plugins_cache.json')
if plugins_cache_generator.found()
    plugins_doc_dep = custom_target('bad-plugins-doc-cache',
        command: [plugins_cache_generator, plugins_cache, '@INPUT@'],
        input: plugins,
        output: 'gst_plugins_cache.json',
    )
else
    warning('GStreamer plugin inspector for documentation not found, can\'t update the cache')
endif

hotdoc_p = find_program('hotdoc', required: get_option('doc'))
if not hotdoc_p.found()
    message('Hotdoc not found, not building the documentation')
    subdir_done()
endif

hotdoc = import('hotdoc')
foreach ext: required_hotdoc_extensions
    if not hotdoc.has_extensions(ext)
        if get_option('doc').enabled()
            error('Documentation enabled but gi-extension missing')
        endif
        message('@0@ extensions not found, not building documentation requiring it'.format(ext))
    endif
endforeach

if not build_gir
    if get_option('doc').enabled()
        error('Documentation enabled but introspection not built.')
    endif

    message('Introspection not built, can\'t build the documentation')
    subdir_done()
endif

build_hotdoc = true

docconf = configuration_data()
docconf.set('GST_API_VERSION', api_version)

configure_file(input : 'gst_api_version.in',
               output : 'gst_api_version.md',
               configuration : docconf)

root_rel = '..'
excludes = []
foreach f: [
        'gst/frei0r/frei0r.h',
        'gst/mxf/mxful.c',
        'gst-libs/gst/mpegts/gstmpegts-private.h',
        'gst-libs/gst/player/*-private.h',
        'gst-libs/gst/codecparsers/nalutils.h',
        'ext/closedcaption/bcd.h',
        'ext/closedcaption/bit_slicer.[ch]',
        'ext/closedcaption/decoder.[ch]',
        'ext/closedcaption/macros.h',
        'ext/closedcaption/misc.h',
        'ext/closedcaption/raw_decoder.[ch]',
        'ext/closedcaption/sampling_par.[ch]',
        'ext/closedcaption/sliced.[ch]',
        'ext/lv2/gstlv2utils.[ch]',
    ]
    excludes += [join_paths(meson.current_source_dir(), root_rel, f)]
endforeach

libs = []
if build_gir
    libs = [
        {'name': 'mpegts', 'gir': mpegts_gir, 'lib': gstmpegts_dep},
        {'name': 'player', 'gir': player_gir, 'lib': gstplayer_dep},
        {'name': 'video', 'gir': badvideo_gir, 'lib': gstbadvideo_dep},
        {'name': 'insertbin', 'gir': insertbin_gir, 'lib': gstinsertbin_dep},
        {'name': 'codecparsers', 'lib': gstcodecparsers_dep},
        {'name': 'adaptivedemux', 'lib': gstadaptivedemux_dep},
        {'name': 'webrtc', 'gir': webrtc_gir, 'lib': gstwebrtc_dep},
   ]
endif

libs_doc = []
foreach lib_def: libs
  name = lib_def['name']
  lib = lib_def['lib']
    if lib_def.has_key('gir')
        if hotdoc.has_extensions('gi-extension')
            gir = lib_def['gir']
            libs_doc += [hotdoc.generate_doc('gst-plugins-bad-' + name,
                project_version: api_version,
                gi_c_sources: [join_paths(root_rel, 'gst-libs/gst', name, '*.[hc]')],
                gi_sources: gir[0].full_path(),
                gi_c_source_filters: excludes,
                sitemap: join_paths('libs', name, 'sitemap.txt'),
                index: join_paths('libs/', name, 'index.md'),
                gi_index: join_paths('libs/', name, 'index.md'),
                gi_smart_index: true,
                gi_order_generated_subpages: true,
                dependencies: [lib, gir],
                c_flags: '-DGST_USE_UNSTABLE_API',
                install: false,
            )]
        endif
    else
        if hotdoc.has_extensions('c-extension')
            libs_doc += [hotdoc.generate_doc('gst-plugins-bad-' + name,
                sitemap: join_paths('libs', name, 'sitemap.txt'),
                index: join_paths('libs', name, 'index.md'),
                project_version: api_version,
                c_sources: [join_paths(root_rel, 'gst-libs/gst/', name, '*.[hc]')],
                c_source_filters: excludes,
                c_index: join_paths('libs', name, 'index.md'),
                c_smart_index: true,
                c_order_generated_subpages: true,
                dependencies: lib,
                c_flags: '-DGST_USE_UNSTABLE_API',
                install: false,
            )]
        endif
    endif
endforeach

list_plugin_res = run_command(python3, '-c',
'''
import sys
import json

with open("@0@") as f:
    print(':'.join(json.load(f).keys()), end='')
'''.format(plugins_cache))

assert(list_plugin_res.returncode() == 0,
  'Could not list plugins from @0@'.format(plugins_cache))

plugins_doc = []
foreach plugin_name: list_plugin_res.stdout().split(':')
    plugins_doc += [hotdoc.generate_doc(plugin_name,
        project_version: api_version,
        sitemap: 'plugins/sitemap.txt',
        index: 'plugins/index.md',
        c_flags: '-DGST_USE_UNSTABLE_API',
        gst_index: 'plugins/index.md',
        gst_smart_index: true,
        gst_c_sources: [
            join_paths(root_rel, 'sys/*/*.[ch]'),
            join_paths(root_rel, 'ext/*/*.[ch]'),
            join_paths(root_rel, 'gst/*/*.[ch]'),
        ],
        dependencies: [gst_dep, plugins],
        extra_extensions_paths: hotdoc_extension_dir,
        install: false,
        disable_incremental_build: true,
        gst_cache_file: plugins_cache,
        gst_c_source_filters: excludes,
        gst_order_generated_subpages: true,
        gst_plugin_name: plugin_name,
        html_theme: html_theme,
    )]
endforeach