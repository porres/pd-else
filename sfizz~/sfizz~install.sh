#!/bin/sh

ext=$1 
objectsdir=$2
basepath=./sfizz~/build/pd/sfizz

mkdir -p $objectsdir
cp $basepath/sfizz~-help.pd $objectsdir
cp $basepath/sfizz.dll $objectsdir/sfizz.$ext
cp $basepath/example.sfz $objectsdir