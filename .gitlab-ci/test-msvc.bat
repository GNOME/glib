@echo on
:: vcvarsall.bat sets various env vars like PATH, INCLUDE, LIB, LIBPATH for the
:: specified build architecture
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64

:: Remove quotes from script args
setlocal enabledelayedexpansion
set args=
for %%x in (%*) do (
  set args=!args! %%~x
)
set args=%args:~1%

:: FIXME: make warnings fatal
pip3 install --upgrade --user meson==1.2.3 packaging==23.2  || goto :error
meson setup %args% _build || goto :error
meson compile -C _build || goto :error

meson test -v -C _build --timeout-multiplier %MESON_TEST_TIMEOUT_MULTIPLIER% || goto :error
meson test -v -C _build --timeout-multiplier %MESON_TEST_TIMEOUT_MULTIPLIER% --setup=unstable_tests --suite=failing --suite=flaky

:: Workaround meson issue https://github.com/mesonbuild/meson/issues/9894
python -c "n = '_build/meson-logs/testlog.junit.xml'; c = open(n, 'rb').read().replace(b'\x1b', b''); open(n, 'wb').write(c)" || goto :error
python -c "n = '_build/meson-logs/testlog-unstable_tests.junit.xml'; c = open(n, 'rb').read().replace(b'\x1b', b''); open(n, 'wb').write(c)"

:: FIXME: can we get code coverage support?

goto :EOF
:error

:: Workaround meson issue https://github.com/mesonbuild/meson/issues/9894
python -c "n = '_build/meson-logs/testlog.junit.xml'; c = open(n, 'rb').read().replace(b'\x1b', b''); open(n, 'wb').write(c)"
python -c "n = '_build/meson-logs/testlog-unstable_tests.junit.xml'; c = open(n, 'rb').read().replace(b'\x1b', b''); open(n, 'wb').write(c)"

exit /b 1
