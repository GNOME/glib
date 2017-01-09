# Common NMake Makefile module for checking the build environment
# This can be copied from $(glib_srcroot)\build\win32 for GNOME items
# that support MSVC builds and introspection under MSVC, and can be used
# for building test programs as well.

# Check to see we are configured to build with MSVC (MSDEVDIR, MSVCDIR or
# VCINSTALLDIR) or with the MS Platform SDK (MSSDK or WindowsSDKDir)
!if !defined(VCINSTALLDIR) && !defined(WINDOWSSDKDIR)
MSG = ^
This Makefile is only for Visual Studio 2008 and later.^
You need to ensure that the Visual Studio Environment is properly set up^
before running this Makefile.
!error $(MSG)
!endif

ERRNUL  = 2>NUL
_HASH=^#

!if ![echo VCVERSION=_MSC_VER > vercl.x] \
    && ![echo $(_HASH)if defined(_M_IX86) >> vercl.x] \
    && ![echo PLAT=Win32 >> vercl.x] \
    && ![echo $(_HASH)elif defined(_M_AMD64) >> vercl.x] \
    && ![echo PLAT=x64 >> vercl.x] \
    && ![echo $(_HASH)endif >> vercl.x] \
    && ![cl -nologo -TC -P vercl.x $(ERRNUL)]
!include vercl.i
!if ![echo VCVER= ^\> vercl.vc] \
    && ![set /a $(VCVERSION) / 100 - 6 >> vercl.vc]
!include vercl.vc
!endif
!endif
!if ![del $(ERRNUL) /q/f vercl.x vercl.i vercl.vc]
!endif

!if $(VCVERSION) > 1499 && $(VCVERSION) < 1600
VSVER = 9
!elseif $(VCVERSION) > 1599 && $(VCVERSION) < 1700
VSVER = 10
!elseif $(VCVERSION) > 1699 && $(VCVERSION) < 1800
VSVER = 11
!elseif $(VCVERSION) > 1799 && $(VCVERSION) < 1900
VSVER = 12
!elseif $(VCVERSION) > 1899 && $(VCVERSION) < 2000
VSVER = 14
!else
VSVER = 0
!endif

!if "$(VSVER)" == "0"
MSG = ^
This NMake Makefile set supports Visual Studio^
9 (2008) through 14 (2015).  Your Visual Studio^
version is not supported.
!error $(MSG)
!endif

VALID_CFGSET = FALSE
!if "$(CFG)" == "release" || "$(CFG)" == "debug"
VALID_CFGSET = TRUE
!endif

# We want debugging symbols logged for all builds,
# using .pdb files for release builds
CFLAGS_BASE = /Zi

!if "$(CFG)" == "release"
CFLAGS_ADD = /MD /O2 $(CFLAGS_BASE)
!else
CFLAGS_ADD = /MDd /Od $(CFLAGS_BASE)
!endif

!if "$(PLAT)" == "x64"
LDFLAGS_ARCH = /machine:x64
!else
LDFLAGS_ARCH = /machine:x86
!endif
