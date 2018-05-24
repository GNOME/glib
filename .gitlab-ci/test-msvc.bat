@echo on
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
@echo on

meson _build || goto :error
ninja -C _build || goto :error

goto :EOF
:error
exit /b 1
