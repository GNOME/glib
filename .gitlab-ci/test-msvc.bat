@echo on
:: vcvarsall.bat sets various env vars like PATH, INCLUDE, LIB, LIBPATH for the
:: specified build architecture
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
@echo on

meson _build || goto :error
ninja -C _build || goto :error

:: FIXME: make fatal
meson test -C _build

:: FIXME: can we get code coverage support?

goto :EOF
:error
exit /b 1
