#!/usr/bin/env python3

# This is in its own file rather than inside meson.build
# because a) mixing the two is ugly and b) trying to
# make special characters such as \n go through all
# backends is a fool's errand.

import sys, os, shutil, subprocess

h_array = ['--fhead',
           "#ifndef __GST_PHOTO_ENUM_TYPES_H__\n#define __GST_PHOTO_ENUM_TYPES_H__\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n",
           '--fprod',
           "\n/* enumerations from \"@filename@\" */\n",
           '--vhead',
           "GType @enum_name@_get_type (void);\n#define GST_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n",
           '--ftail',
           "G_END_DECLS\n\n#endif /* __GST_PHOTO_ENUM_TYPES_H__ */"]

c_array = [
    '--fhead',
    "#include \"photography-enumtypes.h\"\n#include\"photography.h\"\n#define C_ENUM(v) ((gint) v)\n#define C_FLAGS(v) ((guint) v)\n",
    '--fprod',
    "\n/* enumerations from \"@filename@\" */",
    '--vhead',
    "GType\n@enum_name@_get_type (void)\n{\n  static gsize id = 0;\n  static const G@Type@Value values[] = {",
    '--vprod',
    "    { C_@TYPE@(@VALUENAME@), \"@VALUENAME@\", \"@valuenick@\" },",
    '--vtail',
    "    { 0, NULL, NULL }\n  };\n\n  if (g_once_init_enter (&id)) {\n    GType tmp = g_@type@_register_static (\"@EnumName@\", values);\n    g_once_init_leave (&id, tmp);\n  }\n\n  return (GType) id;\n}"
    ]

# [perl, glib-mkenums]
cmd = [sys.argv[1], sys.argv[2]]
ofilename = sys.argv[3]
headers = sys.argv[4:]

if ofilename.endswith('.h'):
    arg_array = h_array
else:
    arg_array = c_array

cmd_array = cmd + arg_array + headers
pc = subprocess.Popen(cmd_array, stdout=subprocess.PIPE)
(stdo, _) = pc.communicate()
if pc.returncode != 0:
    sys.exit(pc.returncode)
open(ofilename, 'wb').write(stdo)
