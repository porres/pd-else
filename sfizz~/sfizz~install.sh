#!/bin/sh

system=$1
ext=$2 
objectsdir=$3
basepath=./sfizz~/build/pd/sfizz

if [ $system = "Windows" ]
then
  baseext=".dll"
elif [ $system = "Darwin" ]
then
  baseext=".pd_darwin"
else
  baseext=".pd_linux"
fi

mkdir -p $objectsdir
cp ./sfz~-help.pd $objectsdir
cp $basepath/sfizz$baseext $objectsdir/sfz~.$ext
cp ./example.sfz $objectsdir

if [ $system = "Windows" ]
then
  strip $objectsdir/sfz~.$ext
fi
