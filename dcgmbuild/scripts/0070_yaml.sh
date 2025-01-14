#!/usr/bin/env bash

set -ex

DIR=$(dirname $(realpath ${0}))
source $DIR/common_for_targets.sh

VERSION=0.7.0

mkdir -p ${HOME}/.build/${TARGET}/yaml_{build,src}
pushd ${HOME}/.build/${TARGET}/yaml_build

download_url "https://github.com/jbeder/yaml-cpp/archive/yaml-cpp-${VERSION}.tar.gz" yaml-cpp.tar.gz
echo "2de0f0ec8f003cd3c498d571cda7a796bf220517bad2dc02cba70c522dddde398f33cf1ad20da251adaacb2a07b77844111f297e99d45a7c46ebc01706bbafb5  yaml-cpp.tar.gz" | sha512sum -c -
tar xzf yaml-cpp.tar.gz -C ${HOME}/.build/${TARGET}/yaml_src --strip-components=1

patch -d ${HOME}/.build/${TARGET}/yaml_src < $DIR/yaml/github-pr-1037.patch

cmake -E env CXXFLAGS='-Wno-unused-variable' CFLAGS='-Wno-unused-variable' \
cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON  \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_STATIC_LIBS=ON \
    -DCMAKE_INSTALL_PREFIX=${PREFIX} \
    ${HOME}/.build/${TARGET}/yaml_src

cmake --build . -j`nproc`
cmake --build . --target install

popd

