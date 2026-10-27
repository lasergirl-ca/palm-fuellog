# FuelLog for Palm OS

A native Palm OS 5 fuel log for the Palm T|X that replaces the paper Fuel Log v2:
fills and charges for several vehicles, loyalty points, service reminders with
alarms, stats, reports and graphs, CSV/XLSX/PDF export to the SD card, CSV/XLSX/XLS
import, backups, and beaming between Palms. Pure 68K code, so it also runs on older
Palms and in CloudpilotEmu.

Proudly made in 🇨🇦 Canada!

Download the latest release candidate from the [Releases](https://github.com/lasergirl-ca/palm-fuellog/releases)
page. FuelLog is also listed on [PalmDB](https://palmdb.net/app/fuellog-for-palmos-5x), where the
announcement and its comments live.

The design lives in the live doc (link in `docs/README.md`); `docs/` holds the
export, the mock-ups, the paper form, the spike write-ups and one file per
milestone (`M1.md` to `M8.md`) with what was built and what to check on the device.

Current state: `v1.0-rc3`. Everything builds and passes the host tests; the M5 to M8
features are waiting on their device pass (see the end of `docs/M8.md`).

## Build

The cross-toolchain (prc-tools-remix 2.3.6, Palm OS SDK 5r3, PilRC 3.2) runs in
a Docker image, because GCC 2.95 does not build natively on Apple Silicon macOS
(see `docs/spikes/SP1.md`).

```
make docker-image   # once: builds the fuellog-toolchain image (amd64, emulated)
make docker         # builds FuelLog.prc inside the image (about 15 s)
make test           # host unit tests with the system C compiler (and the Go reader, if go is installed)
make check          # resource listing and the code-section count
```

Install on the T|X by copying `FuelLog.prc` to `/PALM/Launcher/` on the SD card
and tapping it in the Launcher's card view, or with `pilot-xfer -i FuelLog.prc`.

## Layout

```
fuellog.def       application name, creator ZFlg, the code sections
src/              the app; sections.h says which file goes in which code resource
rsc/              fuellog.rcp, resource IDs, launcher icons
tests/            host unit tests (records, calc, dates, CSV, XLSX and XLS readers, exports...)
tools/            prcinfo.py, sectionize.py, check-sections.sh, mkicons.py, mkxls.py,
                  mkpdb (fixture builder) and fuellogdump (Go reader for backup sets)
toolchain/        Dockerfile for the cross-toolchain
docs/             design spec, mock-ups, paper form, spikes, milestone notes
```

Things that bite on this toolchain: `int` is 16 bits, C89 declaration order, each
code resource is capped at 32 KB (hence the sections), the data segment at 32 KB
(big buffers go on the heap), and a `static const` table inside a sectioned
function is addressed wrongly on the device. `tools/check-sections.sh` fails the
build on cross-section calls that would misbehave.

## Contributing

Pull requests are welcome! Run `make test` and `make docker` before opening one,
and say which device or emulator you tried the change on, if any. By opening a
pull request you agree that your change is licensed under the GPLv3 like the rest.

## Licence

FuelLog is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version. It is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; see `LICENSE` for the full text.

Copyright (C) 2026 Zoe Leto. Contributions are accepted under the same licence.

The design documents, mock-ups and the paper form in `docs/` are also covered
by this licence. prc-tools and the Palm OS SDK are not part of this repository
and keep their own terms.
