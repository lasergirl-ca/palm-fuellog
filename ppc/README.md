# FuelLog for Pocket PC

A Windows Mobile 5 and 6 version of FuelLog, built on the Palm version's own core. The
record formats, the maths, the dates, stats, reports, service rules, points and sync rules
are the files in `../src`, compiled unchanged; this folder adds storage and the windows.

Status: **P1, the fill list.** It opens the six FuelLog databases, shows the fills with the
same columns and figures as the Palm's list, and can take a backup set a Palm wrote to a
storage card. It does not yet add or edit anything.

## How it fits with the Palm version

- **Same data.** The Pocket PC keeps its log as the Palm's own `.pdb` files, in
  `\My Documents\FuelLog`: `FuelLog-ZFlg.pdb`, `FuelVeh-ZFlg.pdb`, `FuelPgm-ZFlg.pdb`,
  `FuelSvT-ZFlg.pdb`, `FuelSvL-ZFlg.pdb` and `FuelDel-ZFlg.pdb`. A backup set from a Palm
  opens here as it is, and a set written here restores on a Palm.
- **Same rules.** `store/db.c` gives the calls of the Palm's `src/db.h` over those files:
  sorted adds, the sync trailer (uid, modified, clock shift) stamped on every write,
  tombstones for deletions. `app/fillview.c` is the Palm fill list's logic.
- **Proved on the devices.** Twelve of the Palm build's host test suites run unchanged on a
  Windows Mobile 5 and a Windows Mobile 6 device (`docs/spikes/SP9.md`).

## Build

```
ppc/dev.sh image          # once: the build box
ppc/dev.sh build          # ppc/build/out/FuelLog.exe (and hello.exe, coretest.exe)
make -C ppc host-test     # the store and the list model, on the Mac
```

The compiler is CeGCC's `arm-mingw32ce` (GCC 9.3.0) in a Docker volume; see `dev.sh` and
`docs/spikes/SP9.md` for where it comes from. `make test` at the top runs the host tests too.

## Install and try

1. Copy `FuelLog.exe` to the device (ActiveSync, or a storage card) and tap it in File
   Explorer. There is no installer yet.
2. With no log on the device it says "No fills yet". To bring the Palm's log across: on the
   Palm, Export, Back up now; put the Palm's card in the Pocket PC; Menu, Use Palm backup.
   It takes the newest set under `\<card>\PALM\Programs\FuelLog\Backup`. Without a card
   slot that fits, copy the `.pdb` files of a set into `\My Documents\FuelLog` instead.
3. The list shows Date, Odo, volume, Total and economy, newest first; the picker at the top
   chooses a vehicle or All vehicles; the footer shows the average and this month's spend.
   About (left soft key) counts the records.

Units are the defaults for now (km, litres, km/L in the list, CAD home currency): Prefs
are a later slice, and the Palm's own Prefs do not travel in a backup set.

## Layout

```
dev.sh, Dockerfile   the build box
Makefile             ARM targets, and host-test for the Mac
store/pdbfile.c      a .pdb file read whole and written whole
store/db.c           FuelLog's six databases over those files
app/fillview.c       what the fill list shows (plain C, host-tested)
app/main.c           the window: list view, vehicle picker, soft-key menu
app/app.rc           the hi-res flag for VGA devices
tests/test_store.c   host tests against the fixture set tools/mkpdb writes
hello/, coretest/    the SP9 spike programmes
```

## Plan

| Slice | What | State |
|---|---|---|
| P1 | The .pdb store; the fill list; taking a Palm backup set | Built; device check pending |
| P2 | New and Edit Fill, with any two of volume, price and total giving the third; delete | |
| P3 | Vehicles, programmes and Prefs (units, currency), kept in the registry | |
| P4 | Stats, reports and graphs | |
| P5 | Service types, visits, the due list and a reminder | |
| P6 | CSV, XLSX and PDF export; CSV, XLSX and XLS import; backup and restore | |
| P7 | Sync with a Palm or another Pocket PC through a `.fuel` file on a card; beaming if IrOBEX allows | |
| P8 | VGA and landscape pass, an icon, a CAB installer | |

## What to check on the devices (P1)

1. `FuelLog.exe` starts, the title bar says FuelLog, and the soft keys read About and Menu.
2. Menu, Use Palm backup finds the Palm's newest set and asks before using it.
3. The list matches the Palm's fill list for the same log: same rows, same figures. The
   economy column is km/L (KPL) whatever the Palm's Prefs say.
4. The vehicle picker narrows the list; an electric car's volume column reads kWh.
5. The footer's month total matches the Palm's for the same vehicle.
6. Opening the on-screen keyboard and closing it leaves the list laid out properly.
7. Starting FuelLog a second time brings the first window forward.
