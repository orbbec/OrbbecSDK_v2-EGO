#!/bin/bash

set -e
##################################################################################

#1.check build dir
if [ ! -d "build" ]; then
    echo "---------------create & check build dir------------------------"
    mkdir build
fi


#2.build
#cd build
pushd build
echo "------------------build start--------------------------------------"

cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_LINUX_ARM64=ON ..
cmake --build . -j16
cmake --install . --prefix ./install



#3.tree display install
echo "------------------tree install dir---------------------------------"
tree -L 1 install
echo "-------------------------------------------------------------------"
echo ""

#3.tree display install
echo "------------------pack tar install dir---------------------------------"
timestr=$(date +"%Y%m%d%H%M%S")
rm -fr *.gz
rm -fr *.txt


#default version
version="2.9.0.1"
#get version from CMakeLists.txt
project_line=$(grep -E '^project\(' ../CMakeLists.txt)
if [[ $project_line =~ VERSION[[:space:]]+([0-9]+\.[0-9]+\.[0-9]+\.[0-9]+) ]]; then
    version="${BASH_REMATCH[1]}"
    echo "get version: $version"
else
    echo "not get version!"
fi



if [ -d "../.git" ]; then
    commit_id=$(git rev-parse HEAD |cut -c 1-8)
    echo "commit_id: $commit_id"
    package_name="OrbbecSDK_v${version}_${timestr}_${commit_id}_linux_x64"
else
    package_name="OrbbecSDK_v${version}_${timestr}_linux_x64"
fi

tar -zcvf ${package_name}.tar.gz install

#generater package md5
md5sum ${package_name}.tar.gz > ${package_name}.md5.txt


echo ""
echo "----------------------------------------------------------------------------------------------------"
ls -lh ./${package_name}.*
echo "----------------------------------------------------------------------------------------------------"

echo ""
popd
