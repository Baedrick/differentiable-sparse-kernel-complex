@echo off
setlocal enabledelayedexpansion
cd /D "%~dp0"

:: --- Usage (2026/10/08) ------------------------------------------------------
::
:: Derived from RADDBG build.bat
::
:: This is a build script for use in Windows development environments. It takes
:: a list of alphanumeric only arguments that specifies the build options. By
:: default if no options are passed, all build targets are built in debug.
::
:: Below is a non-exhaustive list of possible ways to use the script:
:: `build playground`
:: `build playground release`
:: `build playground asan`
::
:: For a full list of possible build targets and their build command lines,
:: search for @build_targets in this file.
::
:: This is a list of all possible command line options:
:: - debug: build in debug
:: - release: build in release
:: - asan: enable address sanitizer
:: - profile: enable address sanitizer
:: - no_shaders: do not compile shaders if they already have been compiled
:: - no_deps: do not compile code dependencies if they already have been compiled

:: --- Unpack Arguments --------------------------------------------------------
for %%a in (%*) do set "%%~a=1"
if not "%msvc%"=="1" if not "%clang%"=="1" set msvc=1
if not "%release%"=="1" set debug=1
if "%debug%"=="1"   set release=0 && echo [debug mode]
if "%release%"=="1" set debug=0 && echo [release mode]
if "%msvc%"=="1"    set clang=0 && echo [msvc compile]
if "%clang%"=="1"   set msvc=0 && echo [clang compile]
if "%~1"==""                     echo [default mode, assuming `playground` build] && set playground=1
if "%~1"=="release" if "%~2"=="" echo [default mode, assuming `playground` build] && set playground=1

:: --- Unpack Command Line Build Arguments -------------------------------------
set compile_flags=
if "%asan%"=="1"    set compile_flags=%compile_flags% -fsanitize=address && echo [asan enabled]
if "%profile%"=="1" set compile_flags=%compile_flags% -DDK_PROFILE_ENABLE && echo [profiling enabled]

:: --- Compile/Link Time Definitions -------------------------------------------
set cl_include=     /I..\src\
set cl_common=      /nologo /FC /Z7 /W4 /WX /std:c++20 /EHsc /Zc:__cplusplus /Fo..\.tmp\ -D_CRT_SECURE_NO_WARNINGS %cl_include% %compile_flags%
set cl_debug=       call cl /Od /MTd /D_DEBUG /RTC1 %cl_common%
set cl_release=     call cl /O2 /MT /DNDEBUG %cl_common%
set cl_link=        /link /MANIFEST:EMBED /INCREMENTAL:NO /pdbaltpath:%%%%_PDB%%%% /noexp /nocoffgrpinfo
set cl_out=         /out:
set cl_obj_out=     /Fo:
set cl_linker=
set clang_include=  -I..\src\
set clang_common=   %clang_include% -std=c++20 -fdiagnostics-absolute-paths -Wall -Werror -Wno-unknown-warning-option -Wno-missing-braces -Wno-unused-function -Wno-unused-parameter -Wno-writable-strings -Wno-missing-field-initializers -Wno-unused-value -Wno-unused-variable -Wno-unused-local-typedef -Wno-deprecated-register -Wno-deprecated-declarations -Wno-unused-but-set-variable -Wno-single-bit-bitfield-constant-conversion -Wno-compare-distinct-pointer-types -Wno-initializer-overrides -Wno-incompatible-pointer-types-discards-qualifiers -Xclang -flto-visibility-public-std -D_USE_MATH_DEFINES -Dstrdup=_strdup -Dgnu_printf=printf -ferror-limit=10000 -mcx16 -msha
set clang_debug=    call clang -g -O0 -D_DEBUG %clang_common% %compile_flags%
set clang_release=  call clang -g -O2 -DNDEBUG %clang_common% %compile_flags%
set clang_link=     -fuse-ld=lld -Xlinker /MANIFEST:EMBED -Xlinker /pdbaltpath:%%%%_PDB%%%% -Xlinker /opt:ref -Xlinker /opt:noicf
set clang_out=      -o
set clang_obj_out=  -o
set clang_linker=   -Xlinker

:: --- Per-Build Settings ------------------------------------------------------
if "%msvc%"=="1"   set linker=%cl_linker%
if "%clang%"=="1"  set linker=%clang_linker%
if "%msvc%"=="1"   set only_compile_flag=/c
if "%clang%"=="1"  set only_compile_flag=-c

:: --- Choose Compile Lines ----------------------------------------------------
if "%msvc%"=="1"     set compile_debug=%cl_debug%
if "%msvc%"=="1"     set compile_release=%cl_release%
if "%msvc%"=="1"     set compile_link=%cl_link%
if "%msvc%"=="1"     set out=%cl_out%
if "%msvc%"=="1"     set obj_out=%cl_obj_out%
if "%clang%"=="1"    set compile_debug=%clang_debug%
if "%clang%"=="1"    set compile_release=%clang_release%
if "%clang%"=="1"    set compile_link=%clang_link%
if "%clang%"=="1"    set out=%clang_out%
if "%clang%"=="1"    set obj_out=%clang_obj_out%
if "%debug%"=="1"    set compile=%compile_debug%
if "%release%"=="1"  set compile=%compile_release%

:: --- Prep Directories --------------------------------------------------------
if not exist bin mkdir bin
if not exist .tmp mkdir .tmp
if not exist src\shaders\.cso mkdir src\shaders\.cso

:: --- Shader Compile Definitions ----------------------------------------------
set fxc=          fxc.exe  /nologo
set fxc_include=  /I..\src\shaders
set fxc_debug=    %fxc% /Od /Zi /WX %fxc_include%
set fxc_release=  %fxc% /O3 /WX /Qstrip_debug /Qstrip_priv %fxc_include%
set fxc_out=      /Fo

:: --- Choose Shader Compile Lines ---------------------------------------------
if "%debug%"=="1"    set fxc_compile=%fxc_debug%
if "%release%"=="1"  set fxc_compile=%fxc_release%

:: --- Build Shaders -----------------------------------------------------------
pushd bin
if "%no_shaders%"=="" (
	echo [building shaders]
	if exist ..\src\shaders\.cso\*.cso del /q ..\src\shaders\.cso\*.cso
	for %%f in (..\src\shaders\*.hlsl) do (
		%fxc_compile% /T vs_5_0 /E vs_main %fxc_out% "..\src\shaders\.cso\%%~nf_vs.cso" "%%f" || exit /b 1
		%fxc_compile% /T ps_5_0 /E ps_main %fxc_out% "..\src\shaders\.cso\%%~nf_ps.cso" "%%f" || exit /b 1
	)
)
popd

:: --- Run Embedding Script ----------------------------------------------------
pushd bin
echo [running embed.ps1]
powershell -NoProfile -ExecutionPolicy Bypass -File ..\embed.ps1 || exit /b 1
popd

:: --- Build (@build_targets) --------------------------------------------------
pushd bin
if "%playground%"=="1" (
	set didbuild=1
	if "%no_deps%"=="" (
		echo [building dependencies]
		%compile% %only_compile_flag% ..\src\thirdparty\imgui\imgui_unity.cpp %obj_out%..\.tmp\imgui.obj || exit /b 1
	)
	echo [building playground]
	%compile% ..\src\playground\playground_main.cpp ..\.tmp\imgui.obj %compile_link% %linker% /NOIMPLIB %out%playground.exe || exit /b 1
)
popd

:: --- Warn On No Builds -------------------------------------------------------
if "%didbuild%"=="" (
	echo [WARNING] no valid build target specified; must use build target names as arguments to this script, like `build playground` or `build playground release`.
	exit /b 1
)
