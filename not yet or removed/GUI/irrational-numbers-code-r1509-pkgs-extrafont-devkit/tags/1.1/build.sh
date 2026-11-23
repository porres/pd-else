#!/bin/bash

# Launcher for building extrafont on Linux/MacOsX
#  using the standard GNU/C tool-chain 
#  Read HOWTOBUILD.txt for further info

_THISCMD=$0
_ARCH=$1


echo =================================
echo Preliminary checks .....
echo =================================

if [ -z "${_ARCH}" ] ; then
	echo "Missing _architecture_: \"x32\" or \"x64\""
	echo "Usage; ${_THISCMD} _architecture_ [args]"
	echo "    optional args are passed to make" 
	exit 1
fi

if [ "${_ARCH}" != "x32"  -a  "${_ARCH}" != "x64" ] ; then
	echo Unsupported architecture "$_ARCH" : Valid values are: x32 or x64
  exit 1
fi

# ... We presume the C-build environment is properly set ..
 

echo =================================
echo Preliminary checks ..... DONE.
echo =================================
echo

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
make ${_MAKEOPTS} -f unix-makefile OS=${_OS} ARCH=${_ARCH}  $*
