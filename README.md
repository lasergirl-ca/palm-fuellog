# FuelLog for Palm OS

A native Palm OS 5 fuel log for the Palm T|X that replaces the paper Fuel Log v2.
The design lives in the live doc (link in `docs/README.md`); `docs/` holds the
export, the mock-ups and the paper form.

## Build

The cross-toolchain (prc-tools-remix 2.3.6, Palm OS SDK 5r3, PilRC 3.2) runs in
a Docker image, because GCC 2.95 does not build natively on Apple Silicon macOS 27
(see SP1 in `docs/spikes/SP1.md`).

```
make docker-image   # once: builds the fuellog-toolchain image (amd64, emulated)
make docker         # builds FuelLog.prc inside the image
make test           # host unit tests with the system C compiler
python3 tools/prcinfo.py FuelLog.prc --expect-code 3   # resource listing and section check
```

Install on the T|X by copying `FuelLog.prc` to `/PALM/Launcher/` on the SD card
and tapping it in the Launcher's card view, or with `pilot-xfer -i FuelLog.prc`.

## Layout

```
fuellog.def       application name, creator ZFlg, code sections
src/              main.c, hello2.c (second code section), calc.c (fixed-point maths)
rsc/              fuellog.rcp and resource IDs
tests/            host unit tests
tools/            prcinfo.py, later the Go PDB reader
toolchain/        Dockerfile for the cross-toolchain
docs/             design spec export, mock-ups, paper form, spike write-ups
```
