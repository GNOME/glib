@echo on
:: vcvarsall.bat sets various env vars like PATH, INCLUDE, LIB, LIBPATH for the
:: specified build architecture
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
@echo on

:: FIXME: make warnings fatal
pip3 install --upgrade --user meson==0.49.2  || goto :error
meson %MESON_OPTIONS% _build || goto :error
ninja -C _build || goto :error

:: FIXME: dont ignore test errors
meson test -C _build --timeout-multiplier %MESON_TEST_TIMEOUT_MULTIPLIER% --no-suite flaky

:: FIXME: can we get code coverage support?


python "%CD%\.gitlab-ci\meson-junit-report.py" --project-name glib ^
--job-id "%CI_JOB_NAME%" --output "%CD%/_build/%CI_JOB_NAME%-report.xml" ^
"%CD%/_build/meson-logs/testlog.json"

goto :EOF
:error
exit /b 1
