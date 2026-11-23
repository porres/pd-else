#!/bin/bash

# Launcher for building extrafont on Linux/MacOsX
#  using the standard GNU/C tool-chain 
#  Read HOWTOBUILD.txt for further info

function Usage {
	echo "Usage: ${_THISCMD} _tcl_ _architecture_ [args]"
	echo "    _tcl_ : \"tcl-8\" or \"tcl-9\""
	echo "    _architecture_ : \"x32\" or \"x64\""
	echo "    optional args are passed to make"
}

_THISCMD=$0
_TCL_MAJOR_VER=$1
_ARCH=$2
shift ; shift

if [ "${_TCL_MAJOR_VER}" != "tcl-8" -a "${_TCL_MAJOR_VER}" != "tcl-9" ] ; then
	Usage
	exit 1
fi

if [ -z "${_ARCH}" ] ; then
	Usage
	exit 1
fi

if [ "${_ARCH}" != "x32"  -a  "${_ARCH}" != "x64" ] ; then
	Usage
	exit 1
fi

if [ "${_TCL_MAJOR_VER}" == "tcl-9"  -a  "${_ARCH}" == "x32" ] ; then
	echo "Unsupported build for tcl-9 and x32"
	exit 1
fi

# ... We presume the C-build environment is properly set ..

 function toLower {
 	echo $* | tr '[:upper:]' '[:lower:]'
 }
_OS=$(uname -s)
_OS=$(toLower $_OS)

case $_OS in
  linux*)  _OS=linux
  ;;
  darwin*) _OS=darwin
  ;;
  *) echo Unsupported OS "$_OS"
     exit 1
     ;;
esac


 ## you can also pass extra args to make ...
shift
_MAKEOPTS="--no-builtin-rules --no-builtin-variables"  ;# you must be EXPLICIT !
make ${_MAKEOPTS} -f unix-makefile OS=${_OS} TCL_MAJOR_VER=${_TCL_MAJOR_VER} ARCH=${_ARCH}  $*
