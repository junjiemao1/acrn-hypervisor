ARG DISTRO
ARG VENDOR

FROM ${VENDOR}:${DISTRO}

ARG DISTRO
ENV DEBIAN_FRONTEND noninteractive

RUN apt-get -y update && \
    apt-get install -y --no-install-recommends ca-certificates

###############################################################################
# For older distros: Add backports packages needed
#  * debhelper dwz libdebhelper-perl: build tool releate update
#  * lintian: to keep an acceptable recent version
#  * linux-libc-dev: acrn-dm needs >= 4.20 for udmabuf UAPI
RUN case ${DISTRO} in \
        buster) \
            echo "deb https://deb.debian.org/debian ${DISTRO}-backports main" > /etc/apt/sources.list.d/${DISTRO}-backports.list; \
            for p in debhelper dwz libdebhelper-perl lintian linux-libc-dev; do \
                (echo "Package: $p"; \
                 echo "Pin: release a=${DISTRO}-backports"; \
                 echo "Pin-Priority: 900"; \
                 echo "") >> /etc/apt/preferences.d/pin-${DISTRO}-backports; \
            done; \
            ;; \
        focal) \
            for p in debhelper dwz libdebhelper-perl lintian; do \
                (echo "Package: $p"; \
                 echo "Pin: release a=${DISTRO}-backports"; \
                 echo "Pin-Priority: 900"; \
                 echo "") >> /etc/apt/preferences.d/pin-${DISTRO}-backports; \
            done; \
            ;; \
    esac

###############################################################################
# Install packages needed for git buildpackage based build 
RUN apt-get -y update && \
    apt-get install -y --no-install-recommends \
        build-essential git-buildpackage devscripts dpkg-dev equivs \
        lintian sudo apt-utils pristine-tar

###############################################################################
# Mount the topdir of the Debian git repository at /source
VOLUME /source/
WORKDIR /source/

###############################################################################
# Get default settings and helper scripts
ADD gbp.conf /etc/git-buildpackage/
ADD debian-pkg-build.sh /usr/local/bin/debian-pkg-build.sh
ADD lintian.sh /usr/local/bin/lintian.sh

###############################################################################
# build Debian packages
ENTRYPOINT ["/usr/local/bin/debian-pkg-build.sh"]