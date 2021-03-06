#!/bin/bash

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

if [ $BUILD_TYPE == android ]; then
  ANDROID_NDK=$ANDROID_NDK $DIR/premake-android.sh -j4
elif [ $BUILD_TYPE == IOS ]; then
    $DIR/premake-ios.sh -j4
elif [ $BUILD_TYPE == native ]; then
  $DIR/premake-make.sh -j4
fi
