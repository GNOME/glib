@echo on

:: Remove quotes from script args
setlocal enabledelayedexpansion
set args=
set stash_plat=0

for %%x in (%*) do (
  if "%%x" == "--plat" set stash_plat=1
  if "!stash_plat!" == "0" set args=!args! %%~x
  if "!stash_plat!" == "1" if /i not "%%x" == "--plat" (set plat=%%x) & (set stash_plat=0)
)
set args=%args:~1%
if "!plat!" == "" set plat=x64
:: vcvarsall.bat sets various env vars like PATH, INCLUDE, LIB, LIBPATH for the
:: specified build architecture
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" !plat!

:: Setup and write a cross-compilation file for ARM64 builds
if not "!plat!" == "x64_arm64" goto :continue_build
set cross_file=vs2019-arm64.txt
echo [host_machine]>!cross_file!
echo system = 'windows'>>!cross_file!
echo cpu_family = 'aarch64'>>!cross_file!
echo cpu = 'arm64'>>!cross_file!
echo endian = 'little'>>!cross_file!
echo.>>!cross_file!
echo [binaries]>>!cross_file!
echo c = 'cl'>>!cross_file!
echo cpp = 'cl'>>!cross_file!
echo c_ld = 'link'>>!cross_file!
echo cpp_ld = 'link'>>!cross_file!
echo ar = 'lib'>>!cross_file!
echo windres = 'rc'>>!cross_file!

echo.>>!cross_file!
echo [properties]>>!cross_file!
echo skip_sanity_check = true>>!cross_file!

set args=%args% --cross-file=!cross_file!

:continue_build
pip3 install --upgrade --user meson==1.4.2 || goto :error
meson setup %args% _build || goto :error
meson compile -C _build || goto :error

:: FIXME: Skip PCRE2 tests for now for 32-bit x86 builds, until we pull in pcre2-10.45 (or so)
:: from our subprojects, as 32-bit Windows Visual Studio tests are fixed upstream
:: Upstream PRs:
:: https://github.com/PCRE2Project/pcre2/pull/480
:: https://github.com/PCRE2Project/pcre2/pull/468
if "!plat!" == "x64_x86" meson test -v -C _build --timeout-multiplier %MESON_TEST_TIMEOUT_MULTIPLIER% --no-suite pcre2 || goto :error
if not "!plat!" == "x64_x86" meson test -v -C _build --timeout-multiplier %MESON_TEST_TIMEOUT_MULTIPLIER% || goto :error
meson test -v -C _build --timeout-multiplier %MESON_TEST_TIMEOUT_MULTIPLIER% --setup=unstable_tests --suite=failing --suite=flaky

:: FIXME: can we get code coverage support?
if not "!cross_file!" == "" if exist !cross_file! del /f/q !cross_file!

goto :EOF
:error
exit /b 1
