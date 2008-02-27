import sys, os, glob, string, re, shutil, fnmatch, inspect

################################################################################
# SCONS HELPER FUNCTIONS
################################################################################
#==============================================================================#

#OS dependent file filters
WIN32_FILTER = re.compile('.*(?:Win32).*', re.I)
STAR_NIX_FILTER = re.compile('.*(?:Unix|Solaris|Irix|Posix|NSPR).*', re.I)

def get_source_files(dirname, ext='.c', platform=None):
    source = [os.path.abspath(f) for f in  glob.glob('%s/*%s' % (dirname, ext))]
    
    #if a platform name is provided, we filter the source files
    if platform:
        sourcefiles = []
        myRE = WIN32_FILTER
        if platform.find('win32') >= 0:
            myRE = STAR_NIX_FILTER
        for x in source:
            if myRE.match(x) == None:
                sourcefiles.append(x)
        source = sourcefiles
    return source


def add_default_options(opts):
    """ Just returns some basic re-usable options """
    from SCons.Options import PathOption
    
    opts.Add('debug', 'Enable debugging', 0)
    opts.Add('optz', 'Set optimizations', 0)
    opts.Add('warnings', 'Enable warnings', 0)
    opts.Add('prefix', 'where to install the files', '0')
    opts.Add('defines', '-D compiler flags', 0)
    opts.Add('include_paths', 'extra include paths', 0)
    opts.Add('lib_paths', 'extra lib paths', 0)
    opts.Add('libs', 'extra libs', 0)
    opts.Add('threading', 'Enable threading', 1)
    opts.Add('verbose', 'Turn on compiler verbose', 1)
    opts.Add('enable64', 'Make a 64-bit build', 0)
    return opts
    


def get_platform():
    """ returns the platform name """
    platform = sys.platform
    if platform != 'win32':
        guess_locs = ['./build/config.guess', './config.guess', '../build/config.guess']
        for loc in guess_locs:
            if not os.path.exists(loc): continue
            try:
                out = os.popen('chmod +x %s' % loc)
                out.close()
                out = os.popen(loc, 'r')
                platform = out.readline()
                platform = platform.strip('\n')
                out.close()
            except:{}
            else:
                break
    return platform


def do_configure(env, dirname='lib'):
    """
    This function essentially mimics the configure script, and sets up
    flags/libs based on the sytem in use. We use the config.guess script
    on all systems but Windows in order to guess the system.
    """
    platform = get_platform()
    _thread_defs="-D_REENTRANT"
    _thread_libs="pthread"
    _cxx_flags=""
    _64_flags=""
    _includes=""
    _cxx_defs=""
    
    opt_warnings = env.subst('$warnings') and int(env.subst('$warnings'))
    opt_debug = env.subst('$debug') and int(env.subst('$debug'))
    opt_64bit = env.subst('$enable64') and int(env.subst('$enable64'))
    opt_threading = env.subst('$threading') and int(env.subst('$threading'))
    opt_verbose = env.subst('$verbose') and int(env.subst('$verbose'))
    
    
    ########################################
    # WIN32
    ########################################
    if platform.find('win32') >= 0:
        _debug_flags="/Zi"
        _warn_flags="/Wall"
        _verb_flags=""
        _64_flags=""
        _optz_med="-O2"
        _optz_fast="-O2"
        _optz_fastest="-O2"
        _optz_flags = _optz_med
        _thread_defs="-D_REENTRANT"
        _thread_libs=""
        
        _cxx_defs="/DWIN32 /UUNICODE /U_UNICODE"
        _cxx_flags="/EHs /GR"
        _cxx_optz_flags=_optz_flags
        _link_libs=""

        # choose the runtime to link against
        # [/MD /MDd /MT /MTd]
        if (env.subst('$CC') != 'gcc'):
            rtflag = '/M'
#            if opt_mex:
#                rtflag += 'D'
#            else:
            rtflag += 'T'
            # debug
            if opt_debug:
                rtflag += 'd'
            _cxx_flags = '%s %s' % (_cxx_flags, rtflag)
    ########################################
    # LINUX
    ########################################
    elif platform.startswith('i686-pc'):
        _debug_flags="-g"
        _warn_flags="-Wall"
        _verb_flags="-v"
        _64_flags="-m64"
        _optz_med="-O1"
        _optz_fast="-O2"
        _optz_fastest="-O3"
        _optz_flags = _optz_med
        _thread_defs="-D_REENTRANT -D__POSIX"
        _thread_libs="pthread"
        
        _cxx_defs="-D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE"
        _cxx_flags=""
        _cxx_optz_flags=_optz_flags
        _link_libs="dl nsl"
#        ar_flags="-ru"
#        dll_flags="-fPIC -shared"
        
    ########################################
    # APPLE 
    ########################################
    elif platform.find('apple') >= 0:
        _debug_flags="-g"
        _warn_flags="-Wall"
        _verb_flags="-v"
        _64_flags="-m64"
        _optz_med="-O1"
        _optz_fast="-O2"
        _optz_fastest="-O3"
        _optz_flags = _optz_med
        _thread_defs="-D_REENTRANT -D__POSIX"
        _thread_libs="pthread"

        _cxx_defs="-D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE"
        _cxx_flags=""
        _cxx_optz_flags=_optz_flags
        _link_libs="dl"


    ########################################
    # SGI
    ########################################
    elif platform.startswith('mips-sgi-'):
#        dll_flags="-shared"
        _debug_flags="-g"
        _warn_flags="-fullwarn"
        _verb_flags="-v"
        _64_flags="-64"
        _optz_med="-O1"
        _optz_fast="-O2"
        _optz_fastest="-O3"
        _optz_flags=_optz_med
        _thread_defs="-D_REENTRANT"
        _thread_libs=""
        
        _cxx_defs="-D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE"
        _cxx_flags="-LANG:std -LANG:ansi-for-init-scope=ON -ptused"
        _cxx_optz_flags=_optz_flags
        _link_libs="m"
#            AR="${CXX}"
#            ar_flags=" -ar -o"
    ########################################
    # SOLARIS
    ########################################
    elif platform.startswith('sparc-sun-'):
        _debug_flags="-g"
        _warn_flags=""
        _verb_flags="-v"
        _64_flags="-xtarget=generic64"
        _optz_med="-xO1"
        _optz_fast="-fast"
        _optz_fastest="-fast"
        _optz_flags=_optz_med
        _thread_defs="-mt"
        _thread_libs="thread"
        
        _cxx_defs="-D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE"
        _cxx_flags=" -instances=static"
        _cxx_optz_flags=_optz_flags
        _link_libs="dl nsl socket"
#        AR="${CXX}"
#        ar_flags="-xar -o"
#        dll_flags="-G -Kpic"
    else:
        print 'Unsupported platform: %s' % platform
        sys.exit(1)

    #snag the defines, if any
    cxx_flags = []
    if env.subst('$defines') and env.subst('$defines') != '0':
        defstr = env.subst('$defines')
        for d in defstr.split(" "):
            cxx_flags.append("-D" + d)
    
    cxx_includes = []
    if env.subst('$include_paths') and env.subst('$include_paths') != '0':
        includeStr = env.subst('$include_paths')
        for d in includeStr.split(";"):
            cxx_includes.append(d)
    
    link_libpath = []
    if env.subst('$lib_paths') and env.subst('$lib_paths') != '0':
        libStr = env.subst('$lib_paths')
        for d in libStr.split(";"):
            link_libpath.append(d)
    link_libs = []
    if env.subst('$libs') and env.subst('$libs') != '0':
        libStr = env.subst('$libs')
        for d in libStr.split(" "):
            link_libs.append(d.strip())
    
    local_lib = '%s/%s' % (dirname, platform)
    if opt_64bit:
        local_lib += '-64'
    # Due to name mangling, if the compiler is GCC, append the gnu
    # but, only append if it doesn't already end with gnu
#    if env.subst('$CC') == 'gcc' and not local_lib.endswith('gnu'):
    if env.subst('$CC') == 'gcc':
        local_lib += '/gnu'
    

    #disable optz if we are debugging
    if opt_debug:
        _cxx_optz_flags = ''
    else:
        _debug_flags = ''
    if not opt_verbose:
        _verb_flags = ''
    if not opt_warnings:
        _warn_flags = ''
    if not opt_64bit:
        _64_flags = ''
   
   
    cxx_includes = cxx_includes
    cxx_defs = _cxx_defs.split() + _thread_defs.split()
    cxx_flags = cxx_flags + _cxx_flags.split() + _optz_flags.split() + _64_flags.split() + _verb_flags.split() + _debug_flags.split() + _warn_flags.split()
    link_libs = link_libs + _link_libs.split() + _thread_libs.split()
    link_libpath = link_libpath

    #save these to the environment, as is, in case we want them later
    env['cxx_includes'] = cxx_includes
    env['cxx_defs'] = cxx_defs
    env['cxx_flags'] = cxx_flags
    env['link_libs'] = link_libs
    env['link_libpath'] = link_libpath
    env['local_lib'] = local_lib
    
    #setup the standard environment variables
    env.Append(CPPPATH = cxx_includes)
    env.Append(CCFLAGS = cxx_flags + cxx_defs)
    env.Append(LIBS = link_libs)
    env.Append(LIBPATH = link_libpath)
    
    #add some variables to the environment that might be helpful
    env['PLATFORM'] = platform
    
    return local_lib


def make_libs(env, build_libs, local_lib='lib', lib_path=None):
    lib_path = (lib_path or []) + [local_lib]
    libs = []
    for lib in build_libs:
        libname = lib['lib']
        if not lib.has_key('source'):
            source = get_source_files(lib.get('dir', 'src'), ext=lib.get('ext', '.c'))
        else:
            source = lib['source']
        depends = []
        if lib.has_key('depends'):
            depends += lib['depends']
        
        if lib.has_key('dynamic') and lib['dynamic']:
            libs.append(env.SharedLibrary(local_lib + '/' + libname, source, LIBS=depends + env['LIBS'], LIBPATH=lib_path + env['LIBPATH']))
        else:
            libs.append(env.StaticLibrary(local_lib + '/' + libname, source, LIBS=depends, LIBPATH=lib_path))
    return libs


#==============================================================================#