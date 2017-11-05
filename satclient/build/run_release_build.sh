# Author: James Beasley
# Repo: https://github.com/embeddedcognition/satclient

#!/bin/bash

#create the obj directory if it doesn't exist
if [ ! -d "obj" ]; then
  mkdir obj
fi

#create the bin directory if it doesn't exist
if [ ! -d "bin" ]; then
  mkdir bin
fi

#create the obj/release directory if it doesn't exist
if [ ! -d "obj/release" ]; then
  mkdir obj/release
fi

#create the obj/release/dependency directory if it doesn't exist
if [ ! -d "obj/release/dependency" ]; then
  mkdir obj/release/dependency
fi

#create the bin/release directory if it doesn't exist
if [ ! -d "bin/release" ]; then
  mkdir bin/release
fi

#clean obj and exe files
#make -f make/release_makefile clean

#run build
make -f make/release_makefile all