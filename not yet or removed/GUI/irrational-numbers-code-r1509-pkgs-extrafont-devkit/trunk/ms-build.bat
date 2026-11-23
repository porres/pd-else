@echo off
setlocal

::  Launcher for building extrafont on Windows.
::   Using MS Visual Studio  (Visual Studio 2015 Community Edition)
::
::  Read HOWTOBUILD.txt for further info


:: =======================================================================

 :: Edit this line if MS Visual Studio is installed in a different directory.
 ::
SET _ctoolsSETUP=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat
:: =======================================================================

goto :start

:Usage
	echo Usage: %_THISCMD% _tcl_ _architecture_ [args]
	echo     _tcl_ : "tcl-8" or "tcl-9"
	echo     _architecture_ ; "x32" or "x64"
	echo     optional args are passed to nmake
	goto :EOF

:start

set _THISCMD=%0
set _TCL_MAJOR_VER=%1
set _ARCH=%2
shift
shift

echo =================================
echo Preliminary checks .....
echo =================================
if "%_TCL_MAJOR_VER%" NEQ "tcl-8" (
	if "%_TCL_MAJOR_VER%" NEQ "tcl-9" (
		call :Usage
		goto :EOF
	)
)

if not DEFINED _TCL_MAJOR_VER (
	call :Usage
	goto :EOF
)

if not DEFINED _ARCH (
	call :Usage
	goto :EOF
)

if not DEFINED _ARCH (
	echo Missing _architecture_: "x32" or "x64"
	echo Usage: %_THISCMD% _architecture_ [args]
	echo     optional args are passed to nmake
	goto :EOF
)

if "%_TCL_MAJOR_VER%" == "tcl-9" (
	if "%_ARCH%" == "x32" (
		echo Unsupported build for "tcl-9" and "x32"
		goto :EOF
	)
)

:: prepare the C-compiler environment with the right _ARCH
if NOT EXIST "%_CTOOLSSETUP%" (
   :: NOTE: don't use parens in this echo block ...
  echo ERROR on setting the C build environment  ...
  echo Currently we expect MS Visual Studio [Community Edition] installed at
  echo   "%_CTOOLSSETUP%%"
  echo If installed at a different location, please adjust the _CTOOLSSETUP variable
  echo in this batch file.
  goto :EOF
)

:: translate _ARCH for CTOOLSSETUP:
::  x32 -> x86
::  x64 -> x64
set _ARCH2=x
if "%_ARCH%" == "x32"  set _ARCH2=x86
if "%_ARCH%" == "x64"  set _ARCH2=x64
if "%_ARCH2%" == "x" (
	echo Unsupported architecture "%_ARCH%" : Valid values are: x32 or x64
	call :Usage
	goto :EOF
)

call "%_CTOOLSSETUP%"  %_ARCH2%
 :: checking if something went wrong is not easy, since the VisualStudio setup
 ::  doesn't return always an error code.
 :: Anyway, first check is on error code
 if errorlevel 1 (
  echo ERROR on setting the C build environment  ...
  echo Something went wrong ... please check if "%_ARCH%" is a valid parameter
  echo   for "%_CTOOLSSETUP%"
  goto :EOF
 )

 ::  other weak checks ...
 if NOT DEFINED VisualStudioVersion (
  echo ERROR on setting the C build environment  ...
  echo Expected a variable named VisualStudioVersion
  echo Something went wrong ... please check if "%_ARCH%" is a valid parameter
  echo   for "%_CTOOLSSETUP%"
  goto :EOF
  )
  :: .. ? other checks ? ...
  
  

echo =================================
echo Preliminary checks ..... DONE.
echo =================================
echo.
   
 :: you can also pass extra args to nmake  ...
nmake -f ms-makefile OS=win TCL_MAJOR_VER=%_TCL_MAJOR_VER% ARCH=%_ARCH% %1 %2 %3 %4 %5 %6 %7 %8 %9

