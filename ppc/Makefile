# FuelLog for Pocket PC, spike SP9: built with CeGCC's arm-mingw32ce in the
# box (dev.sh build) for Windows Mobile 5 and later. ARMv4T code runs on
# every Windows Mobile 5 and 6 device.
CC = arm-mingw32ce-gcc
STRIP = arm-mingw32ce-strip
CFLAGS = -O2 -Wall -march=armv4t -D_WIN32_WCE=0x0501 -DUNICODE -D_UNICODE
LDFLAGS = -Wl,--subsystem,wince
OUT = build/out
OBJ = build/obj

all: $(OUT)/hello.exe $(OUT)/coretest.exe

$(OUT)/hello.exe: hello/hello.c
	@mkdir -p $(OUT)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< -lcoredll
	$(STRIP) $@

# The core self-test: the Palm build's own host test suites (tests/test_*.c,
# the ones that touch no files), compiled unchanged for ARM with main renamed
# per suite and printf collected, then run one after another on the device.
SUITES = calc records units prefs text date stats service report points sync csv
CORE = calc records presets units currency prefs text datecalc stats service report points sync csv outstream
SUITE_OBJS = $(SUITES:%=$(OBJ)/test_%.o)
CORE_OBJS = $(CORE:%=$(OBJ)/%.o)

$(OBJ)/test_%.o: ../tests/test_%.c coretest/ct.h
	@mkdir -p $(OBJ)
	$(CC) $(CFLAGS) -std=gnu99 -include coretest/ct.h -Dmain=suite_$* -Dprintf=ct_printf -c -o $@ $<

$(OBJ)/%.o: ../src/%.c
	@mkdir -p $(OBJ)
	$(CC) $(CFLAGS) -std=gnu99 -c -o $@ $<

$(OUT)/coretest.exe: coretest/coretest.c coretest/ct.h $(SUITE_OBJS) $(CORE_OBJS)
	@mkdir -p $(OUT)
	$(CC) $(CFLAGS) -std=gnu99 $(LDFLAGS) -Wl,--stack,0x40000 -o $@ coretest/coretest.c $(SUITE_OBJS) $(CORE_OBJS) -lcoredll
	$(STRIP) $@

clean:
	rm -rf build

.PHONY: all clean
