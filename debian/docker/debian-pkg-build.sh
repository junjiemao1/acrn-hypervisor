#!/bin/bash -e

if [ ! -f debian/control ]; then
    echo "Cannot find debian/control" >&2
    exit 1
fi

if [[ -n ${UID} && -n ${GID} ]]; then
    addgroup --gid ${GID} docker-build
    adduser --uid=${UID} --gid=${GID} --disabled-password --gecos '' docker-build
else
    echo "UID/GID not set. Use docker run -e UID=$(id -u) -e GID=$(id -g)" >&2
    exit 1
fi

# import local packages (eg. acpica-tools) as APT repo
if compgen -G "/apt/*.deb" > /dev/null; then
    pushd /apt >/dev/null
    apt-ftparchive packages . > Packages
    chown docker-build:docker-build Packages
    apt-ftparchive release . > Release
    chown docker-build:docker-build Release
    popd >/dev/null
    echo "deb [trusted=yes] file:/apt ./" > /etc/apt/sources.list.d/local.list
    apt-get -y update
fi

# install build dependencies
mk-build-deps --tool='apt-get -o Debug::pkgProblemResolver=yes --no-install-recommends --yes' --install debian/control --remove
# fixup for mk-build-deps on Debian bullseye
rm -f $(dpkg-parsechangelog -Ssource)-build-deps_$(dpkg-parsechangelog -Sversion)_*.*

# start build
export HOME=$(echo ~docker-build)
sudo -E -u docker-build gbp buildpackage "$@"
