# FuelLog for Pocket PC

A Windows Mobile 5 and 6 version of FuelLog, built on the Palm version's own core. The
record formats, the maths, the dates, stats, reports, service rules, points and sync rules
are the files in `../src`, compiled unchanged; this folder adds storage and the windows.

Status: **P3b, Programmes and Points.** It opens the six FuelLog databases, shows the
fills with the same columns and figures as the Palm's list, can take a backup set a Palm
wrote to a storage card, adds, edits and deletes fills and charges by the Palm form's rules
with their points, keeps the vehicles and the rewards programmes, and has its own
Preferences for units and currency. P1 to P3 ran on a Windows Mobile 5 and a Windows
Mobile 6 device (Nov 12, 2026); P3b has not run on one yet.

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

5. **Menu, Preferences** sets the units (distance, volume, economy, price per litre or
   gallon, price in dollars or cents), the home currency and the currency a new fill
   starts in. They start at the Palm's defaults (km, litres, cents, CAD), because the
   Palm's own Prefs do not travel in a backup set, and are kept in the registry under
   `HKEY_CURRENT_USER\Software\FuelLog` with the vehicle the list shows and the last
   payment, currency and exchange rates.
6. **Menu, Vehicles** lists the vehicles with their fill count and last odometer; a tap
   opens one, New makes one (sixteen at most), and Show archived brings back the hidden
   ones. A vehicle with fills can be archived but not deleted, as on the Palm.

7. **Menu, Programmes** lists the rewards programmes, most used first, with the balance
   and either what expires by the end of next month or the earn rule. A tap opens Edit
   Programme (the Palm's form: earn rule, payment, auto-apply, redemption, expiry,
   opening balance, import aliases); Menu there archives a programme with fills, restores
   an archived one, or deletes one no fill uses. A new, empty log starts with the Palm's
   six presets, as the Palm does.
8. **Points** on the fill form is a button: it opens the Points page for that fill, up to
   four programmes, each with earned and redeemed points and a hint ("auto: 10 pts/L x
   40.00 L", "redeem: $1.00 off"). OK keeps the rows and takes the redemptions off the
   total; Cancel puts them back as they were. A new fill starts with its auto-apply
   programmes for its payment and currency, as on the Palm.

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
app/vehedit.c        the vehicle list's rows and Edit Vehicle's rules (host-tested)
app/pgmedit.c        programme balances, expiry, the list and Edit Programme's rules (host-tested)
app/settings.c       Prefs as the bytes kept in the registry (host-tested)
app/main.c           the window: its pages (list, fill, Points, Preferences, vehicles, programmes) and soft keys
app/app.rc           the hi-res flag for VGA devices
tests/test_store.c   host tests against the fixture set tools/mkpdb writes
tests/test_filledit.c  host tests of the fill form's rules on a copy of that set
tests/test_vehedit.c   host tests of the Prefs bytes and the vehicle rules
tests/test_pgmedit.c   host tests of the programme rules and the presets
hello/, coretest/    the SP9 spike programmes
```

## Plan

| Slice | What | State |
|---|---|---|
| P1 | The .pdb store; the fill list; taking a Palm backup set | Works on both devices (Nov 12, 2026) |
| P2 | New and Edit Fill, with any two of volume, price and total giving the third; delete | Works on both devices (Nov 12, 2026) |
| P3 | Vehicles; Prefs (units, currency), kept in the registry | Works on both devices (Nov 12, 2026) |
| P3b | Programmes and the Points page on a fill | Built; device check pending |
| P4 | Stats, reports and graphs | |
| P5 | Service types, visits, the due list and a reminder | |
| P6 | CSV, XLSX and PDF export; CSV, XLSX and XLS import; backup and restore | |
| P7 | Sync with a Palm or another Pocket PC through a `.fuel` file on a card; beaming if IrOBEX allows | |
| P8 | VGA and landscape pass, an icon, a CAB installer | |

## What to check on the devices (P3b)

1. Menu reads Vehicles, Programmes, Preferences, Use Palm backup, About, Exit.
2. Programmes: the Palm's programmes with the same balances as the Palm's Programmes
   list; soft keys Done and New. Swipe the list sideways to see the rule or expiry column.
3. A tap opens Edit Programme with the Palm's figures. Redeem None hides the Points and
   value rows; Per volume shows Per, Up to and Cap; Expires After months shows Months;
   Counts in Money relabels Earn and Opening.
4. Change a programme's earn rate, Save: the list's rule follows. Menu, Archive on one
   with fills: it moves to the bottom marked archived; Menu, Restore brings it back.
5. New Fill: the Points button names the auto-apply programme and its points, and follows
   the volume as it is typed (10 a litre for Petro-Points).
6. Tap Points: one row per programme with Earned, Redeemed and a hint. Type 1000 in
   Redeemed for Petro-Points: the hint reads "redeem: $1.00 off" and the line at the
   bottom "Total less $1.00 in points". OK: the fill's total is a dollar less and its hint
   reads "total less points". Save, and edit the fill again: the redemption is kept.
7. On the Points page, Add a programme, then set its Programme back to (none): the row
   goes. Cancel: the fill's points are as before the page opened.
8. A US-dollar fill starts without Petro-Points; a Card-only programme comes and goes
   with Cash and Card.

## What was checked on the devices (P3, Nov 12, 2026: works on both)

1. Menu now reads Vehicles, Preferences, Use Palm backup, About, Exit.
2. Preferences: seven pickers, soft keys Save and Cancel. Change Economy to MPG (US) and
   Distance to miles, Save: the list's Odo column and economy column follow, and the
   footer's average. Cancel changes nothing.
3. Close FuelLog and start it again: the units are still as set, and the vehicle picker
   is on the vehicle it showed.
4. Preferences, Price as Dollars: New Fill's price label reads "Price $/L:" and a typed
   1.789 gives the same total as 178.9 did in cents.
5. Vehicles: each vehicle with its type, fill count and last odometer; soft keys Done and
   New. A tap opens Edit Vehicle with its name, type, starting odometer and tank.
6. In Edit Vehicle, change Type to Electric: the tank row reads Battery kWh, the Fuel row
   goes and Home c/kWh appears. Plug-in hybrid shows both Fuel and Home c/kWh.
7. A vehicle with fills has no Delete in its Menu and says why at the bottom; tick
   Archived, Save: it leaves the list and the pickers, and Show archived brings it back.
8. New vehicle, Save: it is in the list page's picker and in New Fill's. Delete it from
   its Menu (it has no fills).
9. A saved fill's payment and currency are what the next New Fill starts with, even
   after closing and starting FuelLog.

## What was checked on the devices (P2, Nov 12, 2026: works on both)

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
