# FuelLog cross-toolchain: prc-tools-remix 2.3.6 (GCC 2.95 for m68k-palmos),
# Palm OS SDK 5r3 and PilRC 3.2, on Debian bookworm amd64.
# Build:  docker build --platform linux/amd64 -t fuellog-toolchain toolchain
# Use:    docker run --rm --platform linux/amd64 -v "$PWD":/src fuellog-toolchain make
FROM debian:bookworm-slim

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      ca-certificates curl make gcc libc6-dev bison flex \
 && rm -rf /var/lib/apt/lists/*

# prc-tools-remix 2.3.6: the first release that builds with GCC 15 / Clang 21.
RUN curl -fsSL -o /tmp/prc-tools.deb \
      https://github.com/jichu4n/prc-tools-remix/releases/download/v2.3.6/prc-tools-remix_2.3.6.bookworm_amd64.deb \
 && apt-get update \
 && apt-get install -y --no-install-recommends /tmp/prc-tools.deb \
 && rm -f /tmp/prc-tools.deb && rm -rf /var/lib/apt/lists/*

# Palm OS SDK 5r3, where prc-tools looks for it (/opt/palmdev), then palmdev-prep.
RUN mkdir -p /opt/palmdev \
 && curl -fsSL https://github.com/jichu4n/palm-os-sdk/archive/5.0.3.tar.gz | tar xz -C /tmp \
 && mv /tmp/palm-os-sdk-5.0.3/sdk-5r3 /opt/palmdev/sdk-5r3 \
 && rm -rf /tmp/palm-os-sdk-5.0.3 \
 && palmdev-prep

# PilRC 3.2, the resource compiler.
RUN curl -fsSL -o /tmp/pilrc.tar.gz \
      https://downloads.sourceforge.net/project/pilrc/pilrc/3.2/pilrc-3.2.tar.gz \
 && tar xzf /tmp/pilrc.tar.gz -C /tmp \
 && cd /tmp/pilrc-3.2 && ./unix/configure && make && make install \
 && rm -rf /tmp/pilrc-3.2 /tmp/pilrc.tar.gz

WORKDIR /src
CMD ["make"]
