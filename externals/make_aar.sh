#!/bin/bash

root=$1
name=$2
version=$3
lib=$4
include=$5

echo "root: $root"
echo "name: $name"
echo "version: $version"
#echo "lib: $lib"
#echo "include: $include"

root=$root/$name-$version
libs=$root/prefab/modules/$name/libs

mkdir -p "$root/prefab/modules/$name/include"
mkdir -p "$libs/android.arm64-v8a"
mkdir -p "$libs/android.armeabi-v7a"
mkdir -p "$libs/android.x86"
mkdir -p "$libs/android.x86_64"

echo '{"abi":"arm64-v8a","api":21,"ndk":21,"stl":"c++_shared"}' > $libs/android.arm64-v8a/abi.json
echo '{"abi":"armeabi-v7a","api":16,"ndk":21,"stl":"c++_shared"}' > $libs/android.armeabi-v7a/abi.json
echo '{"abi":"x86","api":16,"ndk":21,"stl":"c++_shared"}' > $libs/android.x86/abi.json
echo '{"abi":"x86_64","api":21,"ndk":21,"stl":"c++_shared"}' > $libs/android.x86_64/abi.json

echo -e '{\n    "export_libraries": [],\n    "library_name": null,\n    "android": {\n      "export_libraries": [],\n      "library_name": null\n    }\n}' > $root/prefab/modules/$name/module.json

printf '{\n    "schema_version": 1,\n    "name": "%s",\n    "version": "%s",\n    "dependencies": []\n}' $name $version > $root/prefab/prefab.json

printf '<manifest xmlns:android="http://schemas.android.com/apk/res/android" package="de.badaix.%s" android:versionCode="1" android:versionName="1.0">\n	<uses-sdk android:minSdkVersion="16" android:targetSdkVersion="29"/>\n</manifest>' $name > $root/AndroidManifest.xml

