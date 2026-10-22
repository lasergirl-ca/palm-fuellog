# SP3: Hi-res and the dynamic input area on the T|X

**Question.** Can one form resize when the input area collapses, and draw a line graph at
double density, both ways?

**Answer (Oct 22, 2026): yes, both ways.** Confirmed on the T|X in three screenshots. The sample form is the hello-world
form extended; the desktop stand-in (Mu's Tungsten T3 emulation) was not tried because there
is no T3 ROM on this Mac.

## What the sample does

- `src/device.c` resolves three facts once at launch: hi-res (`sysFtrNumWinVersion` >= 4),
  the screen density from `WinScreenGetAttribute`, and the Pen Input Manager
  (`FtrGet('pins', pinFtrAPIVersion)`).
- On `frmLoadEvent`, when PIN exists: `FrmSetDIAPolicyAttr(frmDIAPolicyCustom)`,
  `PINSetInputTriggerState(pinInputTriggerEnabled)`, `WinSetConstraintsSize(160, 225, 225,
  160, 160, 160)` and `PINSetInputAreaState(pinInputAreaUser)`, in that order.
- `LayoutMainForm` reads `WinGetDisplayExtent`, sets the form window to it, pins the Done
  button to the bottom and gives the graph everything between the labels and the button.
  It runs on `frmOpenEvent` and again on `winDisplayChangedEvent`, which the OS sends when
  the input area opens or collapses; the handler erases the window and redraws.
- `src/graph.c` switches to `kCoordinatesDouble` when hi-res exists, draws axes, the six
  full-to-full economy points from the mock-ups, a dotted mean line and labels in 320-pixel
  coordinates, then restores the coordinate system. Without hi-res it draws the same thing
  at standard density. A pen tap in the plot calls out the nearest point (date and value);
  a second tap clears it.
- A status line under the labels prints the display extent, PIN presence and density, so
  the screenshot says what the device reported.

## What to check on the T|X

1. Launch with the input area open. Expect "Display 160x160, PIN, density 144", the graph
   filling the space above Done, and crisp (hi-res) lines and text in the graph.
2. Tap the input-area trigger in the status bar to collapse it. Expect the form to grow to
   "Display 160x225", Done at the bottom, the graph taller, nothing clipped or stale.
3. Open the input area again. Expect the form back to 160x160 with no leftovers.
4. Tap a point. Expect a ring on it and "Oct 8: 8.1" style text above the plot.
5. Tap Done. Expect a clean return to the Launcher.

A screenshot of each of steps 1, 2 and 4 settles the spike.

## Device result

- Step 1, 11:49 Oct 22: "Display 160x160, PIN, density 144"; the graph, mean line, points and labels drew at double density with the input area open. The two short labels overlapped on one row, fixed by stacking them.
- Step 4, 11:51: a tap near the Sep 15 point ringed it and drew "Sep 15: 8.3" above the plot, kept inside the right axis. The standard-coordinate hit test lines up with the double-density drawing.
- Step 2, 11:53: with the input area collapsed the status line read "Display 160x224, PIN, density 144", Done sat at the bottom edge, the graph used the full height, and a tap called out "Sep 3: 8.2". Note the real extent is 224 rows, not 225: the constraint maximum of 225 is accepted, but the T|X hands the form 448 hi-res rows under its status bar.
- Steps 3 and 5 (reopen, Done) were not screenshotted; the reopen path is the same handler.

## Notes for M2 and M4

- The constraints call fixes the width at 160 and allows heights 160 to 225; the T|X
  actually delivers 224 (448 hi-res rows under its status bar), so layouts must read
  `WinGetDisplayExtent` rather than assume 225. Landscape is out of scope for v1.
- The graph draws straight to the screen here; M4 moves it to an offscreen window created
  under `kCoordinatesDouble` and copies once, as the Look and feel table requires.
- `GraphHitTest` approximates the y-label width at 18 standard pixels; the real graph will
  keep its geometry from the last draw instead.
