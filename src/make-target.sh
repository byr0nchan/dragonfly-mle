#!/bin/sh

if [ -f Makefile ]
then
   OSTYPE=$(uname -s)
   case $OSTYPE in
	Linux)   echo Target is: ${OSTYPE}; sed -i 's/CC = $(BSD_CC)/CC = $(LINUX_CC)/g' Makefile;;
	FreeBSD) echo Target is: ${OSTYPE}; sed -i "" 's/CC = $(LINUX_CC)/CC = $(BSD_CC)/g' Makefile;;
	*)       echo unsupported;;
   esac
else
  echo "Makefile not found"
fi




