#!/bin/bash

# Helper script to build ACRN with docker
# This also includes building packages required for ACRN build or runtime

TOPDIR=$(git rev-parse --show-toplevel)
DOCKER=$(which docker)

if [ -z "${TOPDIR}" ]; then
    echo "Run $0 from inside git repository!"
    exit 1
fi

if [ -z "${DOCKER}" ]; then
    echo "Cannot find docker binary, please install!"
    exit 1
fi

pushd ${TOPDIR} >/dev/null

if [ ! -f debian/docker/Dockerfile ]; then
    echo "No Dockerfile available!"
    exit 1
fi

# use debian stable as default distribution
if [ -z "${DISTRO}" ]; then
    DISTRO=stable
fi

# normalize distro data
# Ubuntu bionic/18.04 is not supported
# Ubuntu hirsute/21.04 EOL -> not supported
case ${DISTRO} in
    buster|oldstable|debian-10)
        VENDOR="debian"
        DISTRO="buster"
        ;;
    bullseye|stable|debian-11)
        VENDOR="debian"
        DISTRO="bullseye"
        ;;
    bookworm|testing|debian-12)
        VENDOR="debian"
        DISTRO="bookworm"
        ;;
    sid|unstable)
        VENDOR="debian"
        DISTRO="sid"
        ;;
    focal|ubuntu-20.04)
        VENDOR="ubuntu"
        DISTRO="focal"
        ;;
    impish|ubuntu-21.10)
        VENDOR="ubuntu"
        DISTRO="impish"
        ;;
    jammy|ubuntu-22.04)
        VENDOR="ubuntu"
        DISTRO="jammy"
        ;;
    *)
        echo "Unsupported distribution ${DISTRO}"
        exit 1
        ;;
esac	

set -e
# create docker image for Debian package build
${DOCKER} build \
    -f debian/docker/Dockerfile \
    --build-arg DISTRO=${DISTRO} \
    --build-arg VENDOR=${VENDOR} \
    -t acrn-pkg-builder:${DISTRO} debian/docker


# helper function to checkout and build required Debian package
# checkout_and_build <url> <debian_tag> <upstream_tag> <build dir> <result dir>
checkout_and_build() {
    url=$1
    debian_tag=$2
    upstream_tag=$3
    builddir=$4
    exportdir=$5

    pkg=$(basename ${url})
    pkg=${pkg%.git}
    echo "Building ${pkg}"
    mkdir -p ${builddir}
    mkdir -p ${exportdir}
    pushd ${builddir} >/dev/null

    # create git repository if needed
    # create git repository if needed
    if [ ! -d .git ] || [ "$(git remote get-url origin)" != "${url}" ]; then
        rm -rf .git
        git init
        git remote add origin ${url}
    else
        echo "Reusing workspace ${builddir}."
    fi
    if [ "$(git describe --exact 2>/dev/null)" != "${debian_tag}" ]; then
        # checkout the required tags only
        git fetch origin --depth 1 refs/tags/${upstream_tag}:refs/tags/${upstream_tag}
        git fetch origin --depth 1 refs/tags/${debian_tag}:refs/tags/${debian_tag}

        # determine debian_branch and pristine-tar
        debian_branch="master" # default value
        # does d/gbp.conf exists?
        if git show ${debian_tag}:debian | grep -qw gbp.conf; then
            # check use of pristine-tar, i.e. pristine_tar = true
            pristine_tar=$(git show ${debian_tag}:debian/gbp.conf | awk -F "=" '/pristine-tar/ {print $2}' | tr '[:upper:]' '[:lower:]' | xargs)
            if [ "${pristine_tar}" == "true" ]; then
                # fetch pristine-tar if necessary
                git fetch origin pristine-tar
                git branch -t pristine-tar origin/pristine-tar
            fi
            # eventually get debian-branch
            debian_branch=$(git show ${debian_tag}:debian/gbp.conf | awk -F "=" '/debian-branch/ {print $2}' | xargs)
            if [ -z "${debian_branch}" ]; then
                debian_branch="master"
            fi
        fi
        # set debian-branch accordingly
        git checkout -b ${debian_branch} ${debian_tag}
    else
        echo "Already checkout out at ${debian_tag}."
    fi

    package=$(grep -E '^Source:' debian/control | awk '{print $2}')
    # get package version w/o using dpkg-parsechanglog
    pkgversion=$(head -1 debian/changelog | awk -F'[()]' '{print $2}')
    if [ ! -f ${exportdir}/${package}_${pkgversion}_*.build ]; then
        # finally start build of the package at ${debian_tag}
        # -F do a full build (binary + source package
        # --no-sign: do not sign the package (TODO implement signing)
        # -git-export-dir=/apt: cumulate all packages in result dir
        # DEB_BUILD_OPTIONS="nocheck": skip test during build to increase build speed
        ${DOCKER} run --rm -e UID=$(id -u) -e GID=$(id -g) \
            -e DEB_BUILD_OPTIONS="nocheck" \
            -v $(pwd):/source \
            -v ${exportdir}:/apt \
            acrn-pkg-builder:${DISTRO} -F --no-sign --git-export-dir=/apt
    else
        echo "No need to build ${package}_${pkgversion}."
    fi
    popd >/dev/null
}

# disto specific handling, e.g. prebuild packages not available/too old in distro
case ${DISTRO} in
    buster|focal)
        # elementpath is needed by python-xmlschema
        checkout_and_build \
            https://salsa.debian.org/debian/elementpath.git \
            debian/2.1.2-1 \
            upstream/2.1.2 \
            build/${DISTRO}/elementpath \
            $(pwd)/build/${DISTRO}
        # python-xmlschema is needed by board-inspector
        checkout_and_build \
            https://salsa.debian.org/python-team/packages/python-xmlschema.git \
            debian/1.4.2-1 \
            upstream/1.4.2 \
            build/${DISTRO}/python-xmlschema \
            $(pwd)/build/${DISTRO}
        # provide a recent acpica-tools package
        checkout_and_build \
            https://github.com/ahs3/acpica-tools.git \
            debian/20200925-1 \
            upstream/20200925 \
            build/${DISTRO}/acpica-tools \
            $(pwd)/build/${DISTRO}
        # that's where the ACRN package build finds the prerequisite packages
        local_apt="-v $(pwd)/build/${DISTRO}:/apt"
        ;;
esac

# build ACRN packages
${DOCKER} run \
    --rm \
    -e UID=$(id -u) \
    -e GID=$(id -g) \
    ${local_apt} \
    -v $(pwd):/source acrn-pkg-builder:${DISTRO} -F --no-sign --git-export-dir=build/${DISTRO} "$@"

popd >/dev/null
