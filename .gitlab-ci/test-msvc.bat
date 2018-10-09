@echo on
:: vcvarsall.bat sets various env vars like PATH, INCLUDE, LIB, LIBPATH for the
:: specified build architecture
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
@echo on

:: FIXME: make warnings fatal
:: FIXME: subproject download is allowed because of libffi
pip3 install --upgrade --user meson==0.48.0  || goto :error
meson %MESON_COMMON_OPTIONS% --wrap-mode=default _build || goto :error
ninja -C _build || goto :error

:: FIXME: dont ignore test errors
meson test -C _build --timeout-multiplier %MESON_TEST_TIMEOUT_MULTIPLIER%

:: FIXME: can we get code coverage support?

goto :EOF
:error
exit /b 1
