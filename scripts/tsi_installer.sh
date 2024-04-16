#!/bin/bash

### TSI Download ###
if git >/dev/null 2>&1; then
    echo "git Available"
    cd ~/
    git clone https://github.com/PanterSoft/tsi.git
elif wget >/dev/null 2>&1; then
    echo "wget Available"
    cd ~/
    # wget https://github.com/PanterSoft/tsi/releases/tag/v0.0.0.tar.gz
    # tar -xvzf v0.0.0.tar.gz
elif curl >/dev/null 2>&1; then
    echo "curl Available"
    cd ~/
    # curl https://github.com/PanterSoft/tsi/releases/tag/v0.0.0.tar.gz
    # tar -xvzf v0.0.0.tar.gz
else
    echo "No Suitable Download method found => Offline install necessary"
    echo "Download the TSI Package manually and Copy to Target System"
    echo "https://github.com/PanterSoft/tsi/releases/tag/v0.0.0.tar.gz"
fi

### Begin of Install ###

cd ~/tsi
source deps_check.sh