#!/bin/sh
# dev.sh: the Pocket PC build box for FuelLog, from macOS or Linux, with the
# repository at /src and CeGCC's arm-mingw32ce in a Docker volume.
#
#   dev.sh image   builds the box (ppc/Dockerfile)
#   dev.sh build   builds what ppc/ holds, into ppc/build/out
#   dev.sh shell   a shell in the box
#
# The toolchain volume is the one the Tessera project built from Max
# Kellermann's cegcc-build (Tessera's wince/dev.sh toolchain). It is mounted
# read-only. Set CEGCC_VOLUME to use another volume holding /opt/cegcc.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
image=fuellog-wince-dev
volume=${CEGCC_VOLUME:-tessera-wince-cegcc}
mounts="-v $root:/src -v $volume:/opt/cegcc:ro"
case "${1:-build}" in
image)
	docker build -t $image "$here"
	;;
build)
	# shellcheck disable=SC2086
	docker run --rm $mounts -w /src/ppc $image sh -c 'mkdir -p build/out && make -s'
	;;
shell)
	# shellcheck disable=SC2086
	docker run --rm -it $mounts -w /src/ppc $image bash
	;;
*)
	echo "usage: dev.sh image|build|shell" >&2
	exit 2
	;;
esac
