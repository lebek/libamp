# vim: set filetype=python :
import os, subprocess, re, sys, os.path

if sys.platform == 'win32':
    # Prefer to find the mingw toolchain first, before MSVC - because MSVC is a
    # pathetic toy and we don't support it.
    env = Environment(tools=['mingw'])
else:
    env = Environment()

# Respect common environment variables that users may want to set
IMPORTED_VARS = ['CC', 'CFLAGS', 'CPPFLAGS', 'CCFLAGS']
for var in IMPORTED_VARS:
    value = os.environ.get(var)
    if value: env[var] = value
env.Append(LINKFLAGS=Split(os.environ.get('LDFLAGS')))

# Libs needed to build test program
TEST_LIBS = ['check', 'm']


COMMON_SOURCES = ['amp.c', 'box.c', 'types.c', 'buftoll.c', 'mem.c',
                  'list.c', 'table.c', 'dispatch.c', 'log.c']


# Because BSD puts things here, and maybe other systems too...
EXTRA_INCLUDE_PATHS = ['/usr/local/include/']
EXTRA_LIB_PATHS     = ['/usr/local/lib/']

for pth in EXTRA_INCLUDE_PATHS:
    if os.path.isdir(pth):
        env.Append(CPPPATH=[pth])

for pth in EXTRA_LIB_PATHS:
    if os.path.isdir(pth):
        env.Append(LIBPATH=[pth])

# Target: `test_amp' executable.
testEnv = env.Clone()

#
# Compiler Flags
#

# Warnings
env.Append(CFLAGS = ['-W',
                     '-Wall',
                     #'-Wstrict-prototypes',
                     #'-Wmissing-prototypes',
                     '-Wshadow',
                     '-Werror',
                     ])

# Optimisation
#env.Append(CCFLAGS = ['-O2'])

# Debug symbols
env.Append(CFLAGS = ['-fdump-tree-ssa'])

# Debug prints
#env.Append(CFLAGS = ['-DDEBUG'])


#
# Build targets
#


# Build test coverage support?
# http://check.sourceforge.net/doc/check_html/check_4.html#SEC20
if os.environ.get('BUILD_COVERAGE_SUPPORT'):
    testEnv.Append(CFLAGS=['-fprofile-arcs', '-ftest-coverage', '-O0'])
    TEST_LIBS.append('gcov')

TEST_SOURCES = COMMON_SOURCES + ['test_amp.c', 'test_types.c', 'test_box.c',
                                 'test_log.c', 'test_list.c', 'test_table.c',
                                 'test_mem.c', 'test_buftoll.c', 'unix_string.c']

# Have we been invoke only to compile coverage files only?
if os.environ.get('COMPILE_COVERAGE'):

    # First produce HTML output.
    htmlDone = False

    cmd = ['geninfo', '.']
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    out, err = p.communicate()
    if p.returncode == 0:
        cmd = (['genhtml', '--output-directory', 'html_coverage'] +
               [os.path.splitext(s)[0]+'.gcda.info' for s in COMMON_SOURCES])
        p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        out, err = p.communicate()
        if p.returncode == 0:
            htmlDone = True
        else:
            print "Failed to generate HTML coverage report running command: %r" % (cmd,)
            print out
            print err
    else:
        print "Failed to generate HTML coverage report running command: %r" % (cmd,)
        print out
        print err


    # Then produce plain-text .gcov file output, and accumulate our own
    # summary information.
    results = {}
    # Looking for output such as:
    # Lines executed:0.00% of 3
    regex = re.compile(r'.*?(\d{1,3}\.\d*)%\s*of\s*(\d*)')
    for src in TEST_SOURCES:
        p = subprocess.Popen(['gcov', '--branch-counts', src],
                             stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE)
        out, err = p.communicate()
        if err:
            sys.stderr.write(err)

        if src in COMMON_SOURCES:
            #match = regex.search(out)
            #if match:
            #    results[src] = (match.group(1), match.group(2))

            # OS X version of gcov also produces coverage output for
            # included header files. Stuff we're interested in is
            # printed last, so..
            match = regex.findall(out)
            if match:
                # this should still work on OSs with a normal gcov
                results[src] = match[-1]
            else:
                print "Failed to parse `gcov' output"
                sys.stdout.write(out)

    print
    print "Coverage summary:"
    print

    maxKeyWidth = max( len(s) for s in results )
    maxPercentWidth = max( len(x[0]) for x in results.values() )

    totalLines = coveredLines = 0
    for key in sorted(results.keys()):
        totalLines += int(results[key][1])
        coveredLines += (float(results[key][0]) / 100) * int(results[key][1])
        spaces = " " * (maxKeyWidth - len(key) + maxPercentWidth - len(results[key][0]))
        print "%s: %s%s%% of %s lines." % ( (key, spaces) + results[key] )

    print
    totPrefix = "Total"
    totalPercent = "%.02f" % (coveredLines / totalLines * 100)
    spaces = " " * (maxKeyWidth - len(totPrefix) + maxPercentWidth - len(totalPercent))
    print "%s: %s%s%% of %s lines." % (totPrefix, spaces, totalPercent, totalLines)
    print
    print "Detailed coverage results are in the .gcov files."
    if htmlDone:
        print "Generated HTML coverage report: html_coverage/index.html"
    sys.exit(0)


# -DAMP_TEST_SUPPORT causes unit-testing support code to be built.
#  * MALLOC macro will initialize memory explicitly to make tests
#    more deterministic
#  * Various objects (AMP_Box, etc) will be built with support for
#    rigging them to fail in order to exercise error-handling code.
testEnv.Append(CFLAGS = ['-DAMP_TEST_SUPPORT'])

# -DNDEBUG causes assert()s to be skipped - currently, we have no way to
# unit-test any of the assert() statements, and leaving them in merely
# clutters up thecode-coverage reports.
testEnv.Append(CFLAGS = ['-DNDEBUG'])

# Always include debug symbols in test program
testEnv.Append(CFLAGS = ['-g'])

# Since our test program does not link to the library, but is compiled directly
# with the libamp object files, the public libamp API symbols should neither be
# decorated as dllexport or dllimport. Setting BUNDLE_LIBAMP causes the AMP_DLL
# macro to be defined as empty.
testEnv.Append(CFLAGS = ['-DBUNDLE_LIBAMP'])

testEnv.Program('test_amp', TEST_SOURCES, LIBS=TEST_LIBS)


# Since we're building the libraries with a different Environment
# (different compiler flags) than the test program, we have to use a seperate
# "namespace" for object files, because they can't be shared with the other
# build targets which are built without -DAMP_TEST_SUPPORT. Scons detects the
# conflict if OBJPREFIX is not set.
env['OBJPREFIX'] = 'lib-'

# If on win32/64 also produce a DEF file, which is used by another build step
# to produce a LIB file, which allows MSVC to link to our DLL. This assumes a
# MingW or compatible compiler.
if env['PLATFORM'] == 'win32':
    env.Append(LINKFLAGS="-Wl,--output-def,amp.def")

# BUILDING_LIBAMP is set so that the header files define the appropriate AMP_DLL
# macro, which is used to decorate public API functions, so that the symbols are
# exported from the shared library/DLL.
env.Append(CFLAGS = ['-DBUILDING_LIBAMP'])

# Do not export symbols by default. This is highly recommened for
# shared-libraries. Symbols that are part of the libamp public
# API are explicitly marked as such via the AMP_DLL macro.
# (Only works on GCC/Mingw >= 4.0)
if 'gcc' in env['TOOLS'] and int(env.get('CCVERSION', '1.0')[0]) >= 4:
    env.Append(CFLAGS = ['-fvisibility=hidden'])


# Target: `libamp' Static Library
env.StaticLibrary('amp', COMMON_SOURCES)


# Target: `libamp' Shared Library
env.SharedLibrary('amp', COMMON_SOURCES)


# Target: `amp.lib' import library. Needed by MSVC compiler to link to a DLL.
if env['PLATFORM'] == 'win32':
    # TODO - Support building LIB file for a 64-bit build of amp.dll
    # (How do we know if we're using a 64-bit compiler?)
    lib_builder = Builder(action='lib.exe /machine:i386 /def:$SOURCE /out:$TARGET')

    # An Environment that lets us find commands via $PATH
    cmdEnv = env.Clone(ENV = {'PATH' : os.environ['PATH']})

    cmdEnv.Append(BUILDERS = {'Lib' : lib_builder})

    # run the builder:
    #          $TARGET    $SOURCE
    cmdEnv.Lib('amp.lib', 'amp.def')

