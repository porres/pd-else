@echo off

::  u2d - X_cmd to DOS
::
::  This program allows to use some basic file-management commands
::   with file-arguments specified in Unix notation.
::
:: examples
::   u2d mkdir  aaa/bbb/ccc         -->  MKDIR aaa\bbb\ccc
::   u2d rmdir  aaa/bbb             -->  RMDIR /S/Q aaa\bbb
::   u2d copy   aaa/*.obj  xxx/yyy  -->  COPY aaa\*.obj xxx\yyy
::   ...

setlocal
set XCMD=%1
shift
set P1=%1
set P2=%2
 :: up to now, no more parameters needed.
 
 :: we assume that parameter P1, P2 are filename in Unix notation;
 :: transform them in DOS natation 
set P1=%P1:/=\%
set P2=%P2:/=\%

if %XCMD% == mkdir (
	MKDIR "%P1%" 2>NUL:
	goto :EOF
)

if %XCMD% == rmdir (
	RMDIR /S/Q "%P1%" 2>NUL:
	goto :EOF
)

if %XCMD% == copy (
	COPY "%P1%" "%P2%"
	goto :EOF
)

if %XCMD% == rm (
	DEL /Q "%P1%"
	goto :EOF
)

if %XCMD% == xcopy (
	XCOPY /S/Y/I /D "%P1%" "%P2%"
	goto :EOF
)

echo *** U2D.bat ERROR *** unsupported xcmd %XCMD%
