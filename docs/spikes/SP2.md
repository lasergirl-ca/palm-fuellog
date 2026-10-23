# SP2: Can GCC 2.95 `long long` and 32-bit fixed point hold lifetime totals without overflow?

**Answer (Oct 22, 2026): yes. Every stored sum fits 32 bits with at least 2.5x headroom at
the worst case; five intermediate products do not, and those go through 64-bit `long long`,
which GCC 2.95 provides on m68k through two libgcc helpers that link cleanly (about 1.6 KB).**

## Worst case

20 years, 4 vehicles, two fills a week each (8,320 fills), 200 L or 150 kWh per fill, $2,000
on one receipt, exchange rate 10.0, 100 points per litre at a 300 % premium multiplier,
1,000,000 km per car.

## Sums (stored or accumulated in 32 bits)

| Quantity | Unit | Worst case | Limit | Headroom |
| --- | --- | --- | --- | --- |
| Fills | count | 8,320 | 4.29e9 | huge |
| Volume, fleet lifetime | mL | 1,664,000,000 | 4,294,967,295 | 2.6x |
| Energy, fleet lifetime | Wh | 1,248,000,000 | 4,294,967,295 | 3.4x |
| Spend, fleet lifetime | cents | 1,664,000,000 | 4,294,967,295 | 2.6x |
| Odometer, one car | km x 10 | 10,000,000 | 4,294,967,295 | 429x |
| Distance, fleet lifetime | km x 10 | 40,000,000 | 4,294,967,295 | 107x |
| Points balance, one programme | pts (`Int32`) | 166,400,000 | 2,147,483,647 | 12.9x |
| Home-currency spend, fleet lifetime at 10.0 | cents | 16,640,000,000 | 4,294,967,295 | **does not fit** |

The last row is the only sum that can pass 32 bits, and only at an absurd combination
(a $16.6M fleet spend converted at 10.0). Decision: home-currency sums stay `UInt32` cents,
which holds $42.9M, and the conversion happens per fill with a 64-bit intermediate. If
that ceiling is ever a concern, the fallback is the one the backlog names: 64-bit accumulators
in reports only.

Realistic figures (one car, 50 fills a year, 50 L, $80) are 30 to 50 times smaller than the
worst case, so the real headroom on every sum is in the hundreds.

## Intermediates

| Product | Worst case | Fits 32 bits? | Where |
| --- | --- | --- | --- |
| price (ten-thousandths/L) x volume (mL) | 42,948,000,000 at $21.474/L x 200 L | **no** (prices moved to ten-thousandths after M1) | `CalcTotalCents` |
| cents x fxRate (x 1e6) | 11,508,320,000 for an ordinary $84.62 at 1.36 | **no** | `CalcHomeCents` |
| 10 x mL for economy, one segment (3 x 200 L) | 6,000,000 | yes | `CalcL100kmX10` |
| 10 x mL for economy, one car lifetime | 4,160,000,000 | barely | `CalcL100kmX10` |
| 10 x mL for economy, fleet lifetime | 16,640,000,000 | **no** | `CalcL100kmX10` |
| earnRate x mL x premiumMult | 600,000,000,000 | **no** | `CalcEarnedPoints` |
| earnRate x mL x 10,000 (per US gallon) | 20,000,000,000,000 | **no** | `CalcEarnedPoints` |
| earnRate x cents x premiumMult | 600,000,000,000 | **no** | `CalcEarnedPoints` |
| 2,352,146 + 50 x l100x10 (MPG) | 2,452,146 | yes | `CalcMpgX10` |

The ordinary currency conversion overflows on the very first CAD/USD fill, so this is not an
edge case. Every helper in `src/calc.c` therefore multiplies in `unsigned long long` and
divides once, rounding to nearest. `CalcMpgX10` stays 32-bit because its bound is small.

## What `long long` costs on the 68k

| Item | Size in code 1 |
| --- | --- |
| `__udivdi3` (64-bit divide) | 1,198 bytes |
| `__muldi3` (64-bit multiply) | about 400 bytes |
| The five calc helpers together | about 500 bytes |

Code resource 1 went from 1,264 to 3,564 bytes with the helpers, the calc library and the
extra label. One 64-bit divide per visible list row or per saved fill is far below the
0.5 s launch and 0.2 s save targets, even under PACE; the M3 stats cache keeps lifetime
figures from being recomputed on every draw.

## Tests

`make test` runs `tests/test_calc.c` on the host: the mock-up sample values (8.4 and 8.1
L/100 km, $84.62, 473 Petro-Points, $1.42 Triangle, 37 cents at 76) and the worst-case rows
above, including the three products that pass 32 bits. The 68k build links `__muldi3` and
`__udivdi3`, and the hello-world form shows `8462 x 1.36 = 11508` computed on the device.

## Decisions

- Storage and sums stay 32-bit as the data model says; no per-year subtotals are needed.
- Intermediates use 64-bit through the `src/calc.c` helpers; no other code multiplies
  stored quantities directly.
- Points round to nearest, which matches the Points dialog mock-up ($1.42 from 141.9 cents).
