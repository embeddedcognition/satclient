# Author: James Beasley
# Repo: https://github.com/embeddedcognition/satclient

#!/bin/bash

#enable proton trace info
export PN_TRACE_FRM=1;

#run sat (signal acquisition & telemetry) client
./build/bin/release/satclient