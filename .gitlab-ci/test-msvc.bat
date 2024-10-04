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

goto :EOF
:error
exit /b 1
