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

#create the obj/test directory if it doesn't exist
if [ ! -d "obj/test" ]; then
  mkdir obj/test
fi

#create the bin/test directory if it doesn't exist
if [ ! -d "bin/test" ]; then
  mkdir bin/test
fi

#run build
make -f make/testcryptoutil_makefile all