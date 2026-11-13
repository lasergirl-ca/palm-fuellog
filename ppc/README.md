# FuelLog for Pocket PC

A Windows Mobile 5 and 6 version of FuelLog, built on the Palm version's own core. The
record formats, the maths, the dates, stats, reports, service rules, points and sync rules
are the files in `../src`, compiled unchanged; this folder adds storage and the windows.

Status: **P2, New and Edit Fill.** It opens the six FuelLog databases, shows the fills with
the same columns and figures as the Palm's list, can take a backup set a Palm wrote to a
storage card, and adds, edits and deletes fills and charges by the Palm form's rules. P1 ran
on a Windows Mobile 5 and a Windows Mobile 6 device (Nov 12, 2026); P2 has not run on one yet.

## How it fits with the Palm version

- **Same data.** The Pocket PC keeps its log as the Palm's own `.pdb` files, in
  `\My Documents\FuelLog`: `FuelLog-ZFlg.pdb`, `FuelVeh-ZFlg.pdb`, `FuelPgm-ZFlg.pdb`,
  `FuelSvT-ZFlg.pdb`, `FuelSvL-ZFlg.pdb` and `FuelDel-ZFlg.pdb`. A backup set from a Palm
  opens here as it is, and a set written here restores on a Palm.
- **Same rules.** `store/db.c` gives the calls of the Palm's `src/db.h` over those files:
  sorted adds, the sync trailer (uid, modified, clock shift) stamped on every write,
  tombstones for deletions. `app/fillview.c` is the Palm fill list's logic and
  `app/filledit.c` the Palm fill form's (`src/fill_form.c`), both without a window, so
  they are tested on the Mac.
- **Proved on the devices.** Twelve of the Palm build's host test suites run unchanged on a
  Windows Mobile 5 and a Windows Mobile 6 device (`docs/spikes/SP9.md`).

## Build

```
ppc/dev.sh image          # once: the build box
ppc/dev.sh build          # ppc/build/out/FuelLog.exe (and hello.exe, coretest.exe)
make -C ppc host-test     # the store, the list and the fill form's rules, on the Mac
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
   Or start a log here: New on an empty log makes a first vehicle, "My car".
3. The list shows Date, Odo, volume, Total and economy, newest first; the picker at the top
   chooses a vehicle or All vehicles; the footer shows the average and this month's spend.
   Menu, About counts the records.
4. **New** (left soft key) opens New Fill; a tap on a row opens that fill. Any two of
   volume, price and total give the third, and the hint beside Price says which was worked
   out. A blank odometer is taken from the trip. **Save** is the left soft key; Menu holds
   Cancel and, for a fill already in the log, Delete. Every save is written to the `.pdb`
   files at once.

Units are the defaults for now (km, litres, km/L in the list, prices in cents, CAD home
currency): Prefs are a later slice, and the Palm's own Prefs do not travel in a backup set.
The last payment, currency and exchange rate are remembered only until FuelLog closes, for
the same reason. Points: a new fill earns its auto-apply programmes as on the Palm, and an
edited fill keeps the points stored on it; the dialog that changes them comes with P3.

Two places where this form deliberately differs from the Palm's, both found while porting:
the previous odometer shown for an edited fill is that vehicle's fill before it (the Palm
takes the record before it, whichever vehicle that is), and emptying a quantity field to
retype it leaves it empty (the Palm refills it from the other two).

## Layout

```
dev.sh, Dockerfile   the build box
Makefile             ARM targets, and host-test for the Mac
store/pdbfile.c      a .pdb file read whole and written whole
store/db.c           FuelLog's six databases over those files
app/fillview.c       what the fill list shows (plain C, host-tested)
app/filledit.c       New and Edit Fill's rules (plain C, host-tested)
app/main.c           the window: the list page, the form page, the soft keys
app/app.rc           the hi-res flag for VGA devices
tests/test_store.c   host tests against the fixture set tools/mkpdb writes
tests/test_filledit.c  host tests of the fill form's rules on a copy of that set
hello/, coretest/    the SP9 spike programmes
```

## Plan

| Slice | What | State |
|---|---|---|
| P1 | The .pdb store; the fill list; taking a Palm backup set | Works on both devices (Nov 12, 2026) |
| P2 | New and Edit Fill, with any two of volume, price and total giving the third; delete | Built; device check pending |
| P3 | Vehicles, programmes, the Points dialog and Prefs (units, currency), kept in the registry | |
| P4 | Stats, reports and graphs | |
| P5 | Service types, visits, the due list and a reminder | |
| P6 | CSV, XLSX and PDF export; CSV, XLSX and XLS import; backup and restore | |
| P7 | Sync with a Palm or another Pocket PC through a `.fuel` file on a card; beaming if IrOBEX allows | |
| P8 | VGA and landscape pass, an icon, a CAB installer | |

## What to check on the devices (P2)

1. The soft keys read New and Menu. A tap anywhere on a row opens Edit Fill with that
   fill's figures; Menu, Cancel goes back and the list is unchanged.
2. New: the title says New Fill, the keyboard comes up with the cursor in Odometer, and the
   hint beside it shows the vehicle's previous reading. With the keyboard up, the form
   scrolls and the field being typed in stays in view, down to Note.
3. Type a volume and a price: Total fills in and the hint says "total calculated". Type the
   total instead of one of them: the third follows. Points shows what the fill earns.
4. Leave Odometer blank and type a trip: the hint says "odo" and a reading, and Save stores
   that reading. Type an odometer that disagrees with the trip: Save asks which to keep.
5. Save: the list comes back with the new fill in date order, and its economy is filled in.
   Close FuelLog and start it again: the fill is still there.
6. Edit a fill, change the date with the date picker, Save: the row moves. Menu, Delete on
   a fill removes it after asking.
7. The date and time pickers work with the stylus; unticking the time stores no time.
8. Currency USD: a Rate row appears, the unit beside Volume changes to US gal, and the
   price label reads per gal. An electric car's form says Energy kWh, Where and Bill.
9. A set edited here still restores on the Palm (copy the six `.pdb` files into a new
   folder under `/PALM/Programs/FuelLog/Backup` on the card; on the Palm, Export,
   Restore, and pick that folder).

## What was checked on the devices (P1, Nov 12, 2026: works on both)

1. `FuelLog.exe` starts, the title bar says FuelLog, and the soft keys read About and Menu.
2. Menu, Use Palm backup finds the Palm's newest set and asks before using it.
3. The list matches the Palm's fill list for the same log: same rows, same figures. The
   economy column is km/L (KPL) whatever the Palm's Prefs say.
4. The vehicle picker narrows the list; an electric car's volume column reads kWh.
5. The footer's month total matches the Palm's for the same vehicle.
6. Opening the on-screen keyboard and closing it leaves the list laid out properly.
7. Starting FuelLog a second time brings the first window forward.
