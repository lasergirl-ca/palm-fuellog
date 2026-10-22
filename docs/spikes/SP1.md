# SP1: Does the jichu4n Homebrew tap build prc-tools on Apple Silicon macOS 27?

**Answer (Oct 22, 2026): not through the tap as it stands. The Docker fallback is the build path, and it works.**
A hello-world `FuelLog.prc` with two code sections builds in about ten seconds inside
the `fuellog-toolchain` image, and `make test` runs on macOS.

## What was tried

| Step | Result |
| --- | --- |
| `brew install prc-tools` (tap pins prc-tools-remix 2.3.4), Oct 12 | Fails at configure: Clang 21 rejects the implicit-int test program in the 2003 autoconf script. The formula also configures the host as `i686-apple-darwin` and sets no `CC` flags. |
| prc-tools-remix 2.3.6 source, built by hand with the project's own `tools/build.sh` flags (`CC="clang -std=gnu89 ..."`, `--host=i686-apple-darwin`), Oct 22 | Gets further: configure passes and binutils 2.14 builds, but the freshly built GCC 2.95 `cc1` dies with signal 11 while compiling `libgcc2.c` (`_muldi3`). That is the 1999 compiler running as a 64-bit arm64 host process, not a missing flag, so it is not worth more of the timebox. Log: scratchpad `logs/native-make.log`. |
| Docker image `toolchain/Dockerfile`: Debian bookworm amd64 with the 2.3.6 `.deb`, Palm OS SDK 5r3 and PilRC 3.2 | Builds in a few minutes under QEMU emulation (Rosetta is off on this Mac). `m68k-palmos-gcc 2.95.3-kgpd`, `pilrc 3.2`, `m68k-palmos-multigen` and `build-prc` all run. |

## What the hello world proves

- `fuellog.def` declares `multiple code { sec2 }`; `src/hello2.c` places a function in that
  section and `src/main.c` calls it to fill a label. The link goes through the multigen
  jump table with no special call syntax.
- `tools/prcinfo.py FuelLog.prc --expect-code 3` lists code 0 (startup stub, 24 bytes),
  code 1 (main, 1,256 bytes) and code 2 (sec2, 40 bytes), type `appl`, creator `ZFlg`.
- The SDK is selected with `-palmos5r3`, the name palmdev-prep derives from the `sdk-5r3`
  directory; `-palmos5` is not a valid name.

## First device run: blank screen, and the fix

The first `.prc` launched on the T|X and showed a white app area with no form. The
call into section 2 had been compiled as `bsrw`, a 16-bit PC-relative branch, because
`hello2.h` declared the function without its section attribute. The linker resolved the
branch as if both sections shared one address space, so on the device it jumped into
the wrong bytes of code resource 1 before `FrmDrawForm` ran. No crash, just nothing.

The prc-tools manual is explicit: the section attribute must be on the declaration that
every caller sees, or "the call will be generated as a call to the default code section".
Two things now enforce that:

- `src/sections.h` defines `SEC2`, and `src/hello2.h` carries it on the prototype.
- `tools/check-sections.sh` runs at link time and fails the build when any object has a
  `DISP16` relocation against a function that lives outside `.text`. Reproducing the
  original mistake makes `make docker` stop with a named file and symbol.

The Makefile also tracks header dependencies now (`-MMD`, with the target path fixed
up because GCC 2.95 writes `main.o:` instead of `build/main.o:`), which is why the first
rebuild after the fix silently reused the stale object.

## Device result

Confirmed on the T|X at 11:36 on Oct 22, 2026: the form draws its title, "Hello from
FuelLog" and "called into code section 2", with the Done button below. The cross-section
call works on the device, which closes SP1 and the hello-world line of the M0 exit criteria.

## Decision

- Day-to-day builds: `make docker` (and `make docker-image` once per machine).
- The native toolchain is a convenience, not a requirement; if it works, `make` with the
  tools on `PATH` builds the same `.prc`.
