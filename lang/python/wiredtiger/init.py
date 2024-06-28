#!/usr/bin/env python
#
# Public Domain 2014-present MongoDB, Inc.
# Public Domain 2008-2014 WiredTiger, Inc.
#
# This is free and unencumbered software released into the public domain.
#
# Anyone is free to copy, modify, publish, use, compile, sell, or
# distribute this software, either in source code form or as a compiled
# binary, for any purpose, commercial or non-commercial, and by any
# means.
#
# In jurisdictions that recognize copyright laws, the author or authors
# of this software dedicate any and all copyright interest in the
# software to the public domain. We make this dedication for the benefit
# of the public at large and to the detriment of our heirs and
# successors. We intend this dedication to be an overt act of
# relinquishment in perpetuity of all present and future rights to this
# software under copyright law.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.
#

# init.py
#      This is installed as __init__.py, and imports the file created by SWIG.
# This is needed because SWIG's import helper code created by certain SWIG
# versions may be broken, see: https://github.com/swig/swig/issues/769 .
# Importing indirectly seems to avoid these issues.
import os, sys
fname = os.path.basename(__file__)
if fname != '__init__.py' and fname != '__init__.pyc':
    print(__file__ + ': this file is not yet installed')
    sys.exit(1)

if sys.version_info[0] <= 2:
    print('WiredTiger requires Python version 3.0 or above')
    sys.exit(1)

# After importing the SWIG-generated file, copy all symbols from it
# to this module so they will appear in the wiredtiger namespace.
me = sys.modules[__name__]
sys.path.append(os.path.dirname(__file__))

# Look for tsan symbols in the .so file in build and hardcode LD_PRELOAD
script_path = os.path.dirname(__file__)
if "build/" in script_path:
    build_path = os.path.dirname(os.path.dirname(os.path.dirname(script_path)))
    so_file = build_path + "/libwiredtiger.so.11.3.0"

    import subprocess
    command = f"nm -D {so_file} | grep tsan"
    result = subprocess.run(command, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    is_tsan_instrumented = result.returncode == 0

    # !!!!!! UPDATE ME: HARDCODED PATH !!!!!!!!!
    os.environ["LD_PRELOAD"] = "/opt/mongodbtoolchain/revisions/11316f1e7b36f08dcdd2ad0640af18f9287876f4/stow/gcc-v4.XAW/lib/gcc/aarch64-mongodb-linux/11.3.0/../../../../lib64/libtsan.so.0"

    # Restart python to have LD_PRELOAD
    if os.environ.get("LD_PRELOAD_SET") != "1":
        os.environ["LD_PRELOAD_SET"] = "1"
        python = sys.executable
        os.execl(python, python, *sys.argv)

# explicitly importing _wiredtiger in advance of SWIG allows us to not
# use relative importing, as SWIG does.  It doesn't work for us with Python2.
import _wiredtiger
import swig_wiredtiger
for name in dir(swig_wiredtiger):
    value = getattr(swig_wiredtiger, name)
    setattr(me, name, value)
