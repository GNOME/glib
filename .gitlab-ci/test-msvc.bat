@echo on
:: vcvarsall.bat sets various env vars like PATH, INCLUDE, LIB, LIBPATH for the
:: specified build architecture
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
@echo on

:: FIXME: make warnings fatal
meson _build || goto :error
ninja -C _build || goto :error

meson test -C _build --timeout-multiplier 4 || goto :error

:: FIXME: can we get code coverage support?

goto :EOF
:error
exit /b 1
