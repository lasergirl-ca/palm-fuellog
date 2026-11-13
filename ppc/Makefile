# FuelLog for Pocket PC, spike SP9: built with CeGCC's arm-mingw32ce in the
# box (dev.sh build) for Windows Mobile 5 and later. ARMv4T code runs on
# every Windows Mobile 5 and 6 device.
CC = arm-mingw32ce-gcc
STRIP = arm-mingw32ce-strip
CFLAGS = -O2 -Wall -march=armv4t -D_WIN32_WCE=0x0501 -DUNICODE -D_UNICODE
LDFLAGS = -Wl,--subsystem,wince
OUT = build/out
OBJ = build/obj

all: $(OUT)/hello.exe $(OUT)/coretest.exe $(OUT)/FuelLog.exe

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

# FuelLog for Pocket PC: the window (app/main.c) over the fill list and fill form
# models, the .pdb store and the shared core.
APP_CORE = calc records presets units currency prefs text datecalc points stats report service
APP_OBJS = $(OBJ)/app_main.o $(OBJ)/app_fillview.o $(OBJ)/app_filledit.o $(OBJ)/app_vehedit.o $(OBJ)/app_pgmedit.o $(OBJ)/app_statsview.o $(OBJ)/app_reportview.o $(OBJ)/app_graphview.o $(OBJ)/app_svcview.o $(OBJ)/app_settings.o $(OBJ)/store_pdbfile.o $(OBJ)/store_db.o $(OBJ)/app_rc.o $(APP_CORE:%=$(OBJ)/%.o)

# _WIN32_IE: the CE headers hide the list view's owner-data and full-row styles
# and InitCommonControlsEx below 0x0500.
$(OBJ)/app_%.o: app/%.c app/fillview.h app/filledit.h app/vehedit.h app/pgmedit.h app/settings.h app/textview.h app/graphview.h app/svcview.h store/db.h store/pdbfile.h
	@mkdir -p $(OBJ)
	$(CC) $(CFLAGS) -D_WIN32_IE=0x0500 -Wextra -std=gnu99 -c -o $@ $<

$(OBJ)/store_%.o: store/%.c store/db.h store/pdbfile.h
	@mkdir -p $(OBJ)
	$(CC) $(CFLAGS) -Wextra -std=gnu99 -c -o $@ $<

$(OBJ)/app_rc.o: app/app.rc
	@mkdir -p $(OBJ)
	arm-mingw32ce-windres -D_WIN32_WCE=0x0501 -o $@ $<

$(OUT)/FuelLog.exe: $(APP_OBJS)
	@mkdir -p $(OUT)
	$(CC) $(LDFLAGS) -Wl,--stack,0x40000 -o $@ $(APP_OBJS) -laygshell -lcommctrl -lcoredll
	$(STRIP) $@

clean:
	rm -rf build

.PHONY: all clean

# Host tests for the parts of the Pocket PC build that are plain C: the .pdb
# store and the fill list model, against the fixture set tools/mkpdb writes.
# Run on the Mac (make -C ppc host-test), not in the box.
HOSTCC ?= cc
HOST = build/host
SHARED = ../src/records.c ../src/calc.c ../src/units.c ../src/currency.c ../src/text.c ../src/datecalc.c ../src/prefs.c ../src/presets.c ../src/points.c ../src/stats.c ../src/report.c ../src/service.c

host-test:
	@mkdir -p $(HOST)
	$(HOSTCC) -Wall -o $(HOST)/mkpdb ../tools/mkpdb/mkpdb.c ../src/records.c ../src/datecalc.c
	@rm -rf $(HOST)/pdb $(HOST)/work && mkdir -p $(HOST)/pdb && $(HOST)/mkpdb $(HOST)/pdb >/dev/null && cp -R $(HOST)/pdb $(HOST)/work
	$(HOSTCC) -Wall -Wextra -o $(HOST)/test_store tests/test_store.c store/pdbfile.c store/db.c app/fillview.c $(SHARED)
	$(HOST)/test_store $(HOST)/pdb $(HOST)/work
	@if command -v go >/dev/null; then (cd ../tools/fuellogdump && go build -o ../../ppc/$(HOST)/fuellogdump .) && $(HOST)/fuellogdump dump $(HOST)/work | diff - ../tests/expected_dump.csv && echo "ok   Go reader reads the set the store wrote"; else echo "skip Go reader (go not installed)"; fi
	@rm -rf $(HOST)/work2 $(HOST)/empty && cp -R $(HOST)/pdb $(HOST)/work2 && mkdir -p $(HOST)/empty
	$(HOSTCC) -Wall -Wextra -o $(HOST)/test_filledit tests/test_filledit.c store/pdbfile.c store/db.c app/filledit.c app/pgmedit.c $(SHARED)
	$(HOST)/test_filledit $(HOST)/work2 $(HOST)/empty
	@if command -v go >/dev/null; then $(HOST)/fuellogdump dump $(HOST)/work2 >/dev/null && $(HOST)/fuellogdump dump $(HOST)/empty >/dev/null && echo "ok   Go reader reads the sets the fill form wrote"; fi
	@rm -rf $(HOST)/work3 && cp -R $(HOST)/pdb $(HOST)/work3
	$(HOSTCC) -Wall -Wextra -o $(HOST)/test_vehedit tests/test_vehedit.c store/pdbfile.c store/db.c app/settings.c app/vehedit.c $(SHARED)
	$(HOST)/test_vehedit $(HOST)/work3
	@if command -v go >/dev/null; then $(HOST)/fuellogdump dump $(HOST)/work3 >/dev/null && echo "ok   Go reader reads the set the vehicle form wrote"; fi
	@rm -rf $(HOST)/work4 $(HOST)/empty4 && cp -R $(HOST)/pdb $(HOST)/work4 && mkdir -p $(HOST)/empty4
	$(HOSTCC) -Wall -Wextra -o $(HOST)/test_pgmedit tests/test_pgmedit.c store/pdbfile.c store/db.c app/pgmedit.c $(SHARED)
	$(HOST)/test_pgmedit $(HOST)/work4 $(HOST)/empty4
	@if command -v go >/dev/null; then $(HOST)/fuellogdump dump $(HOST)/work4 >/dev/null && echo "ok   Go reader reads the set the programme form wrote"; fi
	@rm -rf $(HOST)/work5 && cp -R $(HOST)/pdb $(HOST)/work5
	$(HOSTCC) -Wall -Wextra -o $(HOST)/test_views tests/test_views.c store/pdbfile.c store/db.c app/statsview.c app/reportview.c app/graphview.c app/pgmedit.c $(SHARED)
	$(HOST)/test_views $(HOST)/work5
	@rm -rf $(HOST)/work6 && cp -R $(HOST)/pdb $(HOST)/work6
	$(HOSTCC) -Wall -Wextra -o $(HOST)/test_service tests/test_service.c store/pdbfile.c store/db.c app/svcview.c app/vehedit.c $(SHARED)
	$(HOST)/test_service $(HOST)/work6
	@if command -v go >/dev/null; then $(HOST)/fuellogdump dump $(HOST)/work6 >/dev/null && echo "ok   Go reader reads the set the service forms wrote"; fi

.PHONY: host-test
