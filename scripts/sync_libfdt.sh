#!/usr/bin/bash

script_dir=$(realpath $(dirname $0))
cd $script_dir/..

# check if linux_path is given as argument, or LINUX_SOURCE_PATH is set
if [ -z "$1" ]; then
    if [ -z "$LINUX_SOURCE_PATH" ]; then
        echo "Please set LINUX_SOURCE_PATH or provide the path to the linux source as argument"
        exit 1
    fi
    linux_path=$LINUX_SOURCE_PATH
else
    linux_path=$1
fi

libfdt_src=$linux_path/scripts/dtc/libfdt
libfdt_dst=$(realpath $(dirname $0)/../libs/libfdt++/libfdt)

echo "Syncing libfdt from '$libfdt_src' to '$libfdt_dst'"

# check if libfdt is present in the linux source
if [ ! -d "$libfdt_src" ]; then
    echo "libfdt not found in $libfdt_src"
    exit 1
fi

# check if cwd is sane
if [ ! -d "libs/libfdt++/" ]; then
    echo "insane cwd, make sure you are in the root of the project"
    exit 1
fi

# copy libfdt to the current directory
cp $libfdt_src/*.{h,c} libs/libfdt++/libfdt/

# make MOS-specific changes
patch -p1 <$script_dir/mos-libfdt.patch
