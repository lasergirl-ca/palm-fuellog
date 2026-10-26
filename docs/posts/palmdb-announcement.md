FuelLog 1.0 rc2: a native fuel log for the Palm T|X (and other 68K Palms)

Proudly made in 🇨🇦 Canada!

I have been replacing the paper fuel log that lived in my friend's glovebox with a Palm OS app, and it is at the release-candidate stage. FuelLog is a new native Palm OS 5 app, written in C against the 5r3 SDK with prc-tools, pure 68K, GPLv3, and built on a Mac in 2026.

**Download:** https://github.com/lasergirl-ca/palm-fuellog/releases/tag/v1.0-rc2

**Source:** https://github.com/lasergirl-ca/palm-fuellog

## What it does

- **Fills and charges** for several vehicles, including hybrids and EVs (kWh and price per kWh). Type any two of volume, price and total and it fills in the third. Economy shows as MPG or km/L on the list, L/100 km in Stats.
- **Loyalty points**: programmes like PC Optimum or Petro-Points, with earn rates, redemptions, balances, what the points are worth, and a warning when a lot is about to expire.
- **Service reminders**: oil changes, tires, inspections, whatever you track, by distance or by months, with a projected due date from your driving pace, an alarm that survives a reset, a "Service!" banner on the main list, and a history of every visit with the gaps you actually reached.
- **Stats, reports and graphs**: per vehicle or fleet-wide, by month, year or a custom range, with price, economy and spend graphs drawn at 320x320 on the T|X.
- **SD card export** to CSV, XLSX (opens in Excel and Numbers) and PDF, with a programme filter; **import** from CSV, XLSX and old-style XLS, with a preview that resolves vehicles and programmes before anything is written.
- **Backups** to the card with restore, and **beaming** of the log or an export to another Palm over IR or Bluetooth.
- Fits the T|X screen with the input area open or closed; the 5-way moves a highlight on the lists and centre opens the row.

## Requirements

Palm OS 5 with an SD card for export, import and backups; 68K code, so older Palms and CloudpilotEmu should run it too (not yet tried, see below). Tested on a Palm T|X.

## Installing

Copy `FuelLog-1.0rc2.prc` to `/PALM/Launcher/` on the card and tap it in the Launcher's card view, or install it with `pilot-xfer -i` or your usual HotSync route. First launch walks you through a vehicle, your units and currency.

## Where it stands

This is a release candidate, not 1.0. Entry, vehicles, programmes, stats, reports, graphs, service and the alarm have all been exercised on a T|X. The SD card features, the Excel and PDF files, the importers and beaming pass their tests on the Mac but have had no device time yet, so that is where I would love testers:

- Export a CSV, XLSX or PDF and open it on your computer.
- Import a spreadsheet of old fills.
- Beam the log between two Palms.
- Run it on anything that is not a T|X: a LifeDrive, a Tungsten, an m500, CloudpilotEmu.

Bugs and screenshots to the GitHub issues, or reply here. Pull requests are welcome.

## For the curious

The build runs GCC 2.95.3 inside a Docker image because prc-tools will not build natively on Apple Silicon. The code is split into nine 32 KB code sections, the data segment is at its 32 KB limit, `int` is 16 bits, and PilRC 3.2 turned out to misread every BMP header on a 64-bit host, which is why the launcher icon is written by a Python script. Notes on all of that are in the repository's `docs/` folder.

Credits: Zoë Leto and Andrew Culwell. Licence: GNU GPLv3.
