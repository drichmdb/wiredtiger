#!/usr/bin/env python3

# Generate WiredTiger function prototypes.
import fnmatch, re, os, sys
from dist import compare_srcfile, format_srcfile, source_files
from common_functions import filter_if_fast

if not [f for f in filter_if_fast(source_files(), prefix="../")]:
    sys.exit(0)

def clean_function_name(filename, fn):
    ret = fn.strip()

    # Ignore statics in XXX.c files.
    if fnmatch.fnmatch(filename, "*.c") and 'static' in ret:
        return None

    # Join the first two lines, type and function name.
    ret = ret.replace("\n", " ", 1)

    # If there's no CPP syntax, join everything.
    if not '#endif' in ret:
        ret = " ".join(ret.split())

    # If it's not an inline function, prefix with "extern".
    if 'inline' not in ret and 'WT_INLINE' not in ret:
        ret = 'extern ' + ret

    # Switch to the include file version of any gcc attributes.
    ret = ret.replace("WT_GCC_FUNC_ATTRIBUTE", "WT_GCC_FUNC_DECL_ATTRIBUTE")

    # Everything but void requires using any return value.
    if not re.match(r'(static inline|static WT_INLINE|extern) void', ret):
        ret = ret + " WT_GCC_FUNC_DECL_ATTRIBUTE((warn_unused_result))"

    # If a line ends in #endif, appending a semicolon results in an illegal
    # expression, force an appended newline.
    if re.match(r'#endif$', ret):
        ret = ret + '\n'

    return ret + ';\n'

# Find function prototypes in a file matching a given regex. Cleans the
# function names to the point being immediately usable.
def extract_prototypes(filename, regexp):
    ret = []
    s = open(filename, 'r').read()
    for p in re.findall(regexp, s):
        clean = clean_function_name(filename, p)
        if clean is not None:
            ret.append(clean)

    return ret

# Build function prototypes from a list of files.
def fn_prototypes(ext_fns, int_fns, tests, name):
    for sig in extract_prototypes(name, r'\n[A-Za-z_].*\n__wt_[^{]*'):
        ext_fns.append(sig)

    for sig in extract_prototypes(name, r'\n[A-Za-z_].*\n__wti_[^{]*'):
        int_fns.append(sig)

    for sig in extract_prototypes(name, r'\n[A-Za-z_].*\n__ut_[^{]*'):
        tests.append(sig)

# Write results and compare to the current file.
# Unit-testing functions are exposed separately in their own section to
# allow them to be ifdef'd out.
def output(ext_fns, tests, f):
    tmp_file = '__tmp_prototypes' + str(os.getpid())

    if not os.path.isfile(f):
        # No such file. Write it from scratch
        tfile = open(tmp_file, 'w')
        tfile.write("#pragma once\n\n")

        tfile.write("/* DO NOT EDIT: automatically built by prototypes.py: BEGIN */\n\n")
        for e in sorted(list(set(ext_fns))):
            tfile.write(e)

        tfile.write('\n#ifdef HAVE_UNITTEST\n')
        for e in sorted(list(set(tests))):
            tfile.write(e)
        tfile.write('\n#endif\n')
        tfile.write("\n\n/* DO NOT EDIT: automatically built by prototypes.py: END */\n")

        tfile.close()
    else:
        # File exists. We want to modify the contents
        with open(f, 'r') as file:
            lines = file.readlines()

        # Modify protoypes
        start_line = lines.index('/* DO NOT EDIT: automatically built by prototypes.py: BEGIN */\n')
        end_line = lines.index('/* DO NOT EDIT: automatically built by prototypes.py: END */\n')

        # Safety check: We should always have some functions defined in the file
        assert(start_line + 1 != end_line)

        # All content before START
        new_lines = lines[:start_line + 1]
        new_lines.append("\n") # maintain the new line after START

        # Replace the function prototypes
        for e in sorted(list(set(ext_fns))):
            new_lines.append(e)

        new_lines.append('\n#ifdef HAVE_UNITTEST\n')
        for e in sorted(list(set(tests))):
            new_lines.append(e)
        new_lines.append('\n#endif\n')

        # All content after END
        new_lines.append("\n") # maintain the new line before END
        new_lines.extend(lines[end_line:])

        with open(tmp_file, 'w') as file:
            file.writelines(new_lines)
    format_srcfile(tmp_file)
    compare_srcfile(tmp_file, f)

from collections import defaultdict

# Update generic function prototypes.
def prototypes_extern():
    ext_func_dict = defaultdict(list)
    int_func_dict = defaultdict(list)
    test_dict = defaultdict(list)

    # This is the list of components that have been modularised as part of Q3 and following work.
    # They place header files inside the src/foo folder rather than in src/include
    modularised_components = ["log"]

    for name in source_files():
        if not fnmatch.fnmatch(name, '*.c') + fnmatch.fnmatch(name, '*_inline.h'):
            continue
        if fnmatch.fnmatch(name, '*/checksum/arm64/*'):
            continue
        if fnmatch.fnmatch(name, '*/checksum/loongarch64/*'):
            continue
        if fnmatch.fnmatch(name, '*/checksum/power8/*'):
            continue
        if fnmatch.fnmatch(name, '*/checksum/riscv64/*'):
            continue
        if fnmatch.fnmatch(name, '*/checksum/zseries/*'):
            continue
        if fnmatch.fnmatch(name, '*/checksum/*'):
            # TODO - checksum is a multi-level directory and this script assumes a flat hierarchy.
            # For now throw these functions into extern.h where they were already located
            fn_prototypes(ext_func_dict["include"], int_func_dict["include"], 
                          test_dict["include"], name)
            continue
        if re.match(r'^.*/os_(?:posix|win|linux|darwin)/.*', name):
            # Handled separately in prototypes_os().
            continue
        if fnmatch.fnmatch(name, '*/ext/*'):
            continue

        if fnmatch.fnmatch(name, '../src/*'):
            # NOTE: This assumes a flat directory with no subdirectories
            comp = os.path.basename(os.path.dirname(name))
            if comp not in modularised_components:
                # Non modularised components put all their function prototypes in 
                # src/include/extern.h
                fn_prototypes(ext_func_dict["include"], int_func_dict["include"], 
                              test_dict["include"], name)
            else:
                fn_prototypes(ext_func_dict[comp], int_func_dict[comp], test_dict[comp], name)
        else:
            print(f"Unexpected filepath {name}")
            exit(1)


    for comp in ext_func_dict.keys():
        if comp == "include":
            # All of these functions go in extern.h
            output(ext_func_dict[comp] + int_func_dict[comp], test_dict[comp], 
                   f"../src/include/extern.h")
        else:
            output(ext_func_dict[comp], test_dict[comp], f"../src/{comp}/{comp}.h")
            if len(int_func_dict[comp]) > 0:
                # empty dict for tests. These functions only exist to expose code to unit tests
                output(int_func_dict[comp], {}, f"../src/{comp}/{comp}_private.h")
        
def prototypes_os():
    """
    The operating system abstraction layer duplicates function names. So each 
    os gets its own extern header file.
    """
    ports = 'posix win linux darwin'.split()
    fns = {k:[] for k in ports}
    tests = {k:[] for k in ports}
    for name in source_files():
        if m := re.match(r'^.*/os_(posix|win|linux|darwin)/.*', name):
            port = m.group(1)
            assert port in ports
            # TODO - just using the same fns dict for internal and external
            fn_prototypes(fns[port], fns[port], tests[port], name)

    for p in ports:
        output(fns[p], tests[p], f"../src/include/extern_{p}.h")

prototypes_extern()
prototypes_os()
