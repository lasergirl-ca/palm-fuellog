# FuelLog for Palm OS 5.4.9

Design package for FuelLog, a native Palm T|X app that replaces the paper Fuel Log v2. Exported from the live doc on October 22, 2026.

Live doc (always the newest version): https://claude.ai/artifact/JNvza1rzN7Y9MFkC8Zvpk6

## What's in here

| File | What it is |
| --- | --- |
| `01-Design-Spec.md` / `.pdf` | The full spec: overview, data model, units and currencies, screens, look and feel, reports and stats, service tracking, rewards points, toolchain, import/export/backup/transfer |
| `02-Screen-Designs.md` / `.pdf` | Every form mocked at real T\|X size (320 x 480) with design notes |
| `03-Backlog.md` / `.pdf` | Roadmap, milestones M0 to M8, spikes SP1 to SP8, 36 user stories, definition of done |
| `mockups/` | All mock-ups and diagrams as PNGs; the Markdown files link to these |
| `source/` | The original paper form the app is based on |

## Notes

- The Markdown files are the most complete copy: each mock-up appears inline from `mockups/`.
- The PDFs are print-ready, but `02-Screen-Designs.pdf` drops three mock-ups that were too large for the PDF renderer. Use the PNGs in `mockups/` for those.
- Status columns in the Backlog are exported as plain text ("To do"). Edit them in the live doc to track progress.

## Key decisions at a glance

- Creator ID `ZFlg`; 68k app under PACE, built with prc-tools + PilRC from the jichu4n Homebrew tap
- Metric fixed-point storage; units, price format ($ or cents) and currency switchable for display
- USD, CAD, EUR, AUD per fill, with a home currency for reports and a default currency for new entries
- Multiple vehicles (fuel, hybrid, plug-in, electric), multiple rewards programmes, Cash or Card
- Trip meter reset at every fill, used to cross-check the odometer
- Service tracking by distance and time, with Palm system alarms
- CSV, XLSX and PDF export; CSV, XLSX and XLS import; SD backup and restore; IR and Bluetooth transfer to another Palm
