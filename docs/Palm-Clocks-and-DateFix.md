# Dates on a Palm whose clock has been moved back: a guide for new apps

What FuelLog learned in October 2026 about keeping real dates on Palm OS devices that run
past 2031, written so the next app does not have to rediscover it. Everything here is
general; the FuelLog file names at the end show one working implementation.

## 1. The problem

Palm OS counts time in two ways and both run out:

| Representation | Width | Range | Who uses it |
|---|---|---|---|
| `DateType` (year 7 bits + 1904, month, day) | 16 bits | 1904 to 2031 | Every PIM record, every date picker, `DateToAscii`, prefs |
| Seconds since 1904-01-01 (`TimGetSeconds`) | 32 bits | 1904 to 2040 | The clock, alarms (`AlmSetAlarm`), `TimSecondsToDateTime` |

The clock can be set to 2031 at most. The people keeping these devices alive move the clock
back by a whole number of leap cycles so weekdays still line up: 28 years is the common
choice (10,227 days, exactly 1,461 weeks). A Palm set this way reports 1998 when it is 2026.

Two populations of Palms exist and data moves between them (SD backups, IR beams, HotSync
exports), so an app has to work in real dates on both:

- a clock set back by hand, nothing else changed; every system function returns the old year;
- a clock set back by [DateFix](https://github.com/User7142/palm-datefix), which also patches
  the OS so the built-in apps show real years. This is the mechanism Zoe's T|X uses.

## 2. What DateFix does, exactly

DateFix moves the epoch from 1904 to a start year S, 1932 by default. Internally nothing
changes: the clock, every stored `DateType` and every `TimGetSeconds` value stay in
"internal" years, S..S+127 meaning 1904..2031. It converts the PIM databases once at
install, then patches a handful of system traps so the screen shows real years:

| Function | Under DateFix | So an app must |
|---|---|---|
| `TimSecondsToDateTime`, `TimGetSeconds` | Internal year (1998), weekday corrected | Add the offset yourself to get the real date |
| `TimDateTimeToSeconds`, `AlmSetAlarm` | Unpatched: expects internal years | Subtract the offset before setting an alarm |
| `DateToAscii`, `DateToDOWDMFormat`, `DateTemplateToAscii` | Adds the offset: shows the real year | Pass internal years only. A real year comes out 28 years too late (2054) |
| `DayOfWeek`, `DayOfMonth` | Patched for the offset | Pass internal years only |
| `SelectDay` (the date picker) | Takes and returns internal years, displays real ones | Convert to internal before, back to real after |
| `DateSecondsToDate`, `DateAdjust`, `DateToDays`, `DaysToDate` | Unpatched arithmetic | Fine with either, as long as you are consistent |
| `StrIToA` | Patched on Palm OS 5 only while the picker is on screen (that is how it shows real years) | Nothing; outside the picker it is the normal function |

It tells apps it is installed:

- Feature `FtrGet('DtFx', 1, &offsetYears)` holds S - 1904 (28 by default; always a multiple
  of 4). It is set on both its Palm OS 5 path and its OS 3.5 to 4 path, so this one check
  covers every device it supports.
- Feature `'DtFx'` 0 is a pointer to its installed code; not useful to apps.
- Its saved prefs are creator `'DtFx'`, id 0, version 2: `{ UInt16 version; UInt16
  startYear; Boolean enabled; UInt8 converting; }`. Prefer the feature; prefs survive a
  reset with DateFix disabled.

DateFix also keeps a table of applications that draw years themselves. An app that calls none
of the patched display functions needs no entry.

## 3. The rule that makes it simple

Store real dates, in your own representation, and touch the system's date functions in as
few places as possible. FuelLog stores days since 1904-01-01 as a `UInt16` and does all
calendar arithmetic in its own host-tested code. The system is asked only for:

1. today (one function),
2. a date from the picker (one wrapper),
3. an alarm time (one wrapper).

Each of those three places applies the shift. Nothing else in the app knows a shift exists.

Do not add a "my clock is set back" checkbox. The user has already told the device; detect it.

## 4. Detecting the shift at launch

```c
/* Once at launch, before anything reads the clock. */
if (FtrGet('DtFx', 1, &years) == errNone && years >= 4 && years <= 68 && years % 4 == 0) {
    isDateFix  = true;
    shiftYears = years;
    shiftDays  = (years / 4) * 1461;         /* exact: every 4 years is 1461 days until 2100 */
} else {
    isDateFix  = false;
    TimSecondsToDateTime(TimGetSeconds(), &now);
    shiftDays  = (now.year + 14 < BUILD_YEAR) ? 10227 : 0;   /* set back by hand */
    shiftYears = shiftDays ? 28 : 0;
}
```

- The feature gives the exact offset for any start year the user chose.
- The fallback is a heuristic. The Makefile passes the build year
  (`-DBUILD_YEAR=$(shell date +%Y)`); a device year more than 14 years behind it is taken as a
  28-year shift. A hard-reset clock (2004 or so) also trips it, which is as wrong as the clock
  itself; the fix is to set the clock. It cannot see a clock set back by any amount but 28, or
  one that is a few years off.
- `today = deviceToday + shiftDays`. Keep both available: the raw device day is needed for
  the picker window and for migrating old records.

## 5. The date picker

`SelectDay` is patched by DateFix to speak internal years. Without DateFix it shows whatever
year it is given. One wrapper handles both:

```c
Boolean AppSelectDay(UInt16 *days, const char *title)
{
    DaysToYmd(*days, &y, &m, &d);
    if (isDateFix) y -= shiftYears;              /* real -> internal for DateFix's picker */
    if (!SelectDay(selectDayByDay, &mm, &dd, &yy, title)) return false;
    if (isDateFix) yy += shiftYears;             /* internal -> real */
    *days = Unshift(YmdToDays(yy, mm, dd), deviceToday, shiftDays);
    return true;
}
```

The last line matters on both kinds of device. The picker's "Today" button and its initial
month come from the clock, so a tap on Today returns the device's date (1998). `Unshift`
moves a date forward by the shift when it falls in a window around the device's today
(FuelLog uses three years before to one year after). A real date already outside that
window is left alone. The window makes the operation idempotent: a moved date is out of it.

Without DateFix a real year handed to `SelectDay` is simply shown; nothing to convert.

## 6. Alarms

`AlmSetAlarm` wants seconds from `TimDateTimeToSeconds`, which DateFix does not patch, and the
clock itself is in internal time on both kinds of device. So take the shift off first:

```c
DaysToYmd(days - shiftDays, &y, &m, &d);   /* device time */
dt.year = y; dt.month = m; dt.day = d; dt.hour = hour; ...
AlmSetAlarm(cardNo, dbID, 0, TimDateTimeToSeconds(&dt), true);
```

Any alarm date you persist in prefs for later comparison should be in your own real-date
representation, not in seconds.

## 7. Records written before the app knew

An earlier build on a shifted Palm has stored device dates (1998). Migrate once per launch and
after every restore, in the same window as the picker:

- walk every dated record; a date inside `[deviceToday - 3y, deviceToday + 1y]` moves forward
  by the shift;
- re-sort the database if any moved (`DmQuickSort`);
- do the same to any dates kept in prefs (FuelLog's last backup date);
- the window makes it safe to run every launch: moved records are out of it, and a real date
  stored on a correct-clock Palm is nowhere near the window either.

Backups and beams between a shifted Palm and a normal one then carry real dates in both
directions. Only artefacts named from the clock before this change (a backup set called
`1998-...`) stay old, and they simply sort as old.

## 8. Show it in About

One short diagnostic saves a support exchange: FuelLog's About line ends in
`DateFix +28y` or `clock +28y` when a shift is in effect, and nothing otherwise. Add the
build stamp and year while you are there.

## 9. Test it on the host

All of the arithmetic is plain C with no Palm calls, so it can run in the host test suite:

- 28 years = 10,227 days; 56 years = the number of days from 1970-01-01 to 2026-01-01;
- the heuristic trips at build year - 15 and not at build year - 14;
- `Unshift` moves a date inside the window and leaves one outside; `shift == 0` is identity;
- with the DateFix years, the picker round trip real -> internal -> real is the identity.

What needs the device: that About says `DateFix +28y`, that the picker opens on the real
month and stores it, that the alarm fires on the right day, that old records read right after
the first launch. CloudpilotEmu with a clock set to 1998 covers the hand-set case; DateFix
itself has to be installed on the emulator image to cover the other.

## 10. Checklist for a new app

- [ ] Own date type (days since 1904 or similar); all calendar maths in host-tested code.
- [ ] Exactly one function returns today; it adds the shift.
- [ ] `AppClockInit()` at launch: `FtrGet('DtFx', 1)` first, build-year heuristic second.
- [ ] One picker wrapper: internal years for DateFix, `Unshift` on the result.
- [ ] Alarms set in device time (shift subtracted).
- [ ] Never call `DateToAscii`, `DayOfWeek`, `DateTemplateToAscii` with a real year; format
      dates yourself or pass internal years.
- [ ] Idempotent migration of previously stored dates at launch and after restore.
- [ ] About shows the shift.
- [ ] Nothing in prefs about it: no checkbox.

## Where this lives in FuelLog

| Piece | File |
|---|---|
| `DateClockShift`, `DateEpochOffsetDays`, `DateUnshift`, day arithmetic | `src/datecalc.c`, tests in `tests/test_date.c` |
| `AppClockInit`, `AppToday`, `AppDeviceToday`, `AppSelectDay`, `AppAlarmSeconds`, `AppClockIsDateFix` | `src/app.c` |
| `DbShiftClockDates` (record migration) | `src/db.c`, called from `src/main.c` and `src/backup.c` |
| Build year define | `Makefile` (`FUELLOG_BUILD_YEAR`) |
| About diagnostics | `src/fill_list.c` |
| Project history of the change | `docs/M8.md`, "After rc3: DateFix and clocks set back" |
