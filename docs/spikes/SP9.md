# SP9: Can FuelLog's core build and run on Windows Mobile 5 and 6?

**Status (Nov 12, 2026): passed on both devices.** The core self-test reports 12 of 12
suites, 457 checks and no failures on a Windows Mobile 5 device and on a Windows Mobile 6
device, with output identical to the Mac's. Closed for the core; the user interface is the
next question.

## The question

A Pocket PC version of FuelLog is worth doing only if the Palm build's portable C (record
formats, maths, dates, stats, reports, service rules, points, sync rules, CSV) carries over
unchanged, and if a Mac can build for the devices at all.

## Toolchain

CeGCC's `arm-mingw32ce`, GCC 9.3.0, built from Max Kellermann's cegcc-build. It already
exists on this Mac: the Tessera project built it into the Docker volume
`tessera-wince-cegcc`, and `ppc/dev.sh` mounts that volume read-only in a box of its own
(`ppc/Dockerfile`, image `fuellog-wince-dev`, native arm64, so no emulation). A machine
without Tessera needs that volume built once with Tessera's `wince/dev.sh toolchain`, or
`CEGCC_VOLUME` pointed at another volume holding `/opt/cegcc`.

Flags, as Tessera's Pocket PC work settled them: `-march=armv4t -D_WIN32_WCE=0x0501
-DUNICODE -D_UNICODE`, linked with `-Wl,--subsystem,wince -lcoredll`. The result is a PE32
ARM executable, Windows CE subsystem version 3.0, importing only COREDLL.dll.

```
ppc/dev.sh image    # once
ppc/dev.sh build    # ppc/build/out/hello.exe and coretest.exe
```

## What was built

- `hello.exe` (6 KB, `ppc/hello/hello.c`): a message box with the Windows CE version, the
  screen size and the memory, also written beside the programme as `hello.txt`.
- `coretest.exe` (78 KB, `ppc/coretest/`): twelve of the Palm build's host test suites
  (`tests/test_calc.c`, `records`, `units`, `prefs`, `text`, `date`, `stats`, `service`,
  `report`, `points`, `sync`, `csv`), compiled for ARM **unchanged**, with `main` renamed per
  suite and `printf` collected by `-D` flags. It runs them one after another, shows one line
  per suite and a count, and writes everything they printed as `coretest.txt`. The fifteen
  source files of the core they exercise (`src/calc.c` to `src/outstream.c`) compiled without
  a change and with no warnings; the only two warnings are deliberate truncations inside the
  tests.

Not in the self-test: the four suites that read fixture files (`inflate`, `xlsxread`,
`xlsread`, `export`). Their code compiles the same way; they need files on the device.

## To run on each device

1. Copy `hello.exe` and `coretest.exe` to the device, by ActiveSync or on a storage card.
2. In File Explorer, tap `hello.exe`. Expected: "FuelLog for Pocket PC: hello", the CE
   version (5.1 for Windows Mobile 5, 5.2 for Windows Mobile 6), the screen size.
3. Tap `coretest.exe`. Expected: twelve lines ending in "ok" and "12 of 12 suites passed".
   If any line says FAILED, `coretest.txt` beside the programme has the detail.

## Device results (Nov 12, 2026)

| Device | hello | coretest |
|---|---|---|
| Windows Mobile 5 | Windows CE 5.1 build 1702, screen 240 x 320, 57,944 KB memory with 36,880 KB free | 12 of 12 suites, 457 checks, 0 failures |
| Windows Mobile 6 | Windows CE 5.2 build 1239, screen 240 x 320, 54,632 KB memory with 29,356 KB free | 12 of 12 suites, 457 checks, 0 failures |

The two `coretest.txt` files are byte-identical. Against the same twelve suites run on the
Mac, all 481 lines match; six differ only in how a value above 2^31 prints through a
32-bit `long` on ARM (for example 3237998081 on the Mac, -1056969215 on the device: the
same bits). So the record formats, the fixed-point maths, the dates, stats, reports,
service rules, points, sync rules and CSV behave on Windows Mobile exactly as on the Palm
build's host tests. Results are kept from the card as Zoe returned them (`WM5/`, `WM6/`).

## What Tessera's Pocket PC work already settled

Zoe's Tessera project (`~/Code/Tessera/wince`, build plan Phase 5) has taken the same road,
and its answers carry over:

- **The toolchain runs on a device.** A CeGCC hello-world ran on an HP iPAQ under Windows
  Mobile 5, and Tessera's shared C core passed every test vector there. FuelLog's two
  programmes are built with the same compiler and flags, so the device run here is a
  confirmation, not an open question.
- **Storage as Palm database files.** Tessera's Pocket PC client keeps the handheld's
  databases as `.pdb` files in `\My Documents\Tessera` (for example `Tessera Root.pdb`),
  through a small load-and-save layer (`palm/emu/pdb.c`: `pdb_load`, `pdb_save`, `pdb_add`,
  `pdb_put`, `pdb_del`). FuelLog's plan is the same shape: its six databases as `.pdb`
  files in `\My Documents\FuelLog`, behind the calls `src/db.h` already declares.
- **The window pattern** (`wince/ppc/app.c`): one top-level window sized to the work area
  (`SPI_GETWORKAREA`), plain child controls (`STATIC`, `EDIT`, `BUTTON`, `LISTBOX`) shown and
  hidden per page, text converted at the boundary (`MultiByteToWideChar` in, 
  `WideCharToMultiByte` out), the on-screen keyboard raised and lowered with
  `SHSipPreference` on edit focus, and one instance only (`FindWindowW`, then
  `SetForegroundWindow` on the handle with its low bit set). Linked with `-laygshell
  -lcoredll` and `-Wl,--stack,0x40000`.
- **Where files are found.** `\Storage Card`, a folder of the app's on it, and
  `\My Documents`; paths are absolute and use backslashes (Windows CE has no current
  directory).
- **Small traps.** A message box cuts long text off under the menu bar, so results that run
  long go in a full-screen read-only edit box (`wince/common/report.c`). A PE object aligns
  to 8 bytes at most. Some desktop Win32 calls are missing (`SecureZeroMemory`).
- **Distribution.** Tessera ships the bare `.exe`, copied to the device; no CAB yet.

Not covered by Tessera, so still new ground for FuelLog: the soft-key menu bar
(`SHCreateMenuBar`), a date picker and combo boxes, drawing graphs with GDI, VGA screens
(the hi-res flag, without which a VGA device pixel-doubles the window), resizing when the
keyboard opens, an alarm (`CeSetUserNotificationEx`), and beaming. Tessera's plan notes that
infrared between a Palm and a Pocket PC depends on both speaking IrOBEX and is untested.

## What this does not answer yet

- The user interface: windows, the soft-key menu bar, the on-screen keyboard, QVGA and VGA.
- Storage: the plan is the Palm's own `.pdb` files and `.fuel` sync files, so a Palm and a
  Pocket PC sync through a card.
- Installation as a CAB, the service alarm, beaming.
