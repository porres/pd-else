@echo off
setlocal

::  Launcher for building extrafont on Windows.
::   Using MS Visual Studio  (Visual Studio 2015 Community Edition)
::   Support for x86/x64 target architecture.
::
::  Read HOWTOBUILD.txt for further info


:: =======================================================================

 :: Edit this line if MS Visual Studio is installed in a different directory.
 :: 
set _CTOOLSSETUP=%ProgramFiles(x86)%\Microsoft Visual Studio 14.0\VC\vcvarsall.bat

:: =======================================================================


set _THISCMD=%0
set _ARCH=%1
shift

echo =================================
echo Preliminary checks .....
echo =================================

if not DEFINED _ARCH (
	echo Missing _architecture_: "x32" or "x64"
	echo Usage: %_THISCMD% _architecture_ [args]
	echo     optional args are passed to nmake 
	goto :EOF
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
nmake -f ms-makefile OS=win ARCH=%_ARCH% %1 %2 %3 %4 %5 %6 %7 %8 %9
