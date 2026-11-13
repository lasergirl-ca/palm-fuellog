# The Pocket PC build box: Ubuntu 24.04 with what CeGCC's arm-mingw32ce
# cross compiler (GCC 9.3.0 for Windows CE and Windows Mobile) is built
# from and runs on. The toolchain itself lives in a Docker volume mounted
# at /opt/cegcc; see dev.sh.
FROM ubuntu:24.04
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
	build-essential bison flex texinfo libgmp-dev libmpfr-dev libmpc-dev git ca-certificates \
	&& rm -rf /var/lib/apt/lists/*
ENV PATH=/opt/cegcc/bin:$PATH
