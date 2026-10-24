# FuelLog for Palm OS. `make` builds FuelLog.prc with prc-tools on PATH;
# `make docker` does the same inside the toolchain image; `make test` runs the
# host unit tests with the system C compiler.

APP      := FuelLog
DEF      := fuellog.def
CC       := m68k-palmos-gcc
MULTIGEN := m68k-palmos-multigen
PILRC    := pilrc
BUILDPRC := build-prc
HOSTCC   := cc

# The build stamp shows in About, so a device can be matched to a build.
BUILD_STAMP := $(shell date +%m%d-%H%M)
CFLAGS   := -O2 -Wall -palmos5r3 -Irsc -Isrc -MMD -DFUELLOG_BUILD='"$(BUILD_STAMP)"'
BUILD    := build

SRCS := src/main.c src/hello2.c src/calc.c src/device.c src/graph.c src/vfstest.c src/sp6test.c \
        src/m1test.c src/db.c src/prefs.c src/records.c src/units.c src/currency.c \
        src/app.c src/fill_list.c src/fill_form.c src/text.c src/datecalc.c \
        src/vehicles.c src/prefs_form.c src/programmes.c src/presets.c src/setup.c src/stats.c src/stats_form.c src/service.c src/service_form.c src/report.c src/reports_form.c src/graphs_form.c src/csv.c src/card.c src/export.c src/backup.c src/export_form.c src/import.c src/export2.c src/xlsxread.c \
        src/outstream.c src/xlsx.c src/pdf.c src/crc32.c src/inflate.c src/zipread.c
OBJS := $(patsubst src/%.c,$(BUILD)/%.o,$(SRCS))
SECT := $(BUILD)/fuellog-sections

all: $(APP).prc

# A failed section check must not leave a half-linked executable behind.
.DELETE_ON_ERROR:

$(BUILD):
	mkdir -p $(BUILD)

# GCC 2.95 writes the -MMD dependency file into the current directory and
# names the target without its directory, so move it and fix the target.
$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
	sed -e 's|^$*\.o:|$@:|' $*.d > $(BUILD)/$*.d && rm -f $*.d

-include $(OBJS:.o=.d)

$(SECT).s $(SECT).ld: $(DEF) | $(BUILD)
	$(MULTIGEN) -b $(SECT) $(DEF)

$(BUILD)/sections.o: $(SECT).s
	$(CC) -c $< -o $@

$(BUILD)/$(APP): $(OBJS) $(BUILD)/sections.o $(SECT).ld
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(BUILD)/sections.o $(SECT).ld
	tools/check-sections.sh $@ $(OBJS)

$(BUILD)/rcp.stamp: rsc/fuellog.rcp rsc/resources.h | $(BUILD)
	$(PILRC) -q -I rsc rsc/fuellog.rcp $(BUILD)
	touch $@

$(APP).prc: $(DEF) $(BUILD)/$(APP) $(BUILD)/rcp.stamp
	$(BUILDPRC) -o $@ $(DEF) $(BUILD)/$(APP) $(BUILD)/*.bin

EXPORT_SRCS := src/outstream.c src/xlsx.c src/pdf.c src/crc32.c
INFLATE_EXTRA ?= docs/spikes/samples/sp6.xlsx

test: $(BUILD)
	$(HOSTCC) -Wall -Wextra -o $(BUILD)/test_calc tests/test_calc.c src/calc.c
	$(BUILD)/test_calc
	$(HOSTCC) -Wall -Wextra -o $(BUILD)/test_records tests/test_records.c src/records.c src/presets.c src/calc.c
	$(BUILD)/test_records
	$(HOSTCC) -Wall -Wextra -o $(BUILD)/test_units tests/test_units.c src/units.c src/currency.c src/calc.c src/records.c
	$(BUILD)/test_units
	$(HOSTCC) -Wall -Wextra -o $(BUILD)/test_prefs tests/test_prefs.c src/prefs.c
	$(BUILD)/test_prefs
	$(HOSTCC) -Wall -Wextra -o $(BUILD)/test_text tests/test_text.c src/text.c
	$(BUILD)/test_text
	$(HOSTCC) -Wall -Wextra -o $(BUILD)/test_date tests/test_date.c src/datecalc.c
	$(BUILD)/test_date
	$(HOSTCC) -Wall -Wextra -o $(BUILD)/test_stats tests/test_stats.c src/stats.c src/calc.c src/datecalc.c
	$(BUILD)/test_stats
	$(HOSTCC) -Wall -Wextra -o $(BUILD)/test_service tests/test_service.c src/service.c src/calc.c src/datecalc.c
	$(BUILD)/test_service
	$(HOSTCC) -Wall -Wextra -o $(BUILD)/test_report tests/test_report.c src/report.c src/calc.c src/datecalc.c
	$(BUILD)/test_report
	$(HOSTCC) -Wall -Wextra -o $(BUILD)/test_csv tests/test_csv.c src/csv.c src/units.c src/currency.c src/calc.c src/datecalc.c src/outstream.c src/records.c
	$(BUILD)/test_csv
	$(HOSTCC) -Wall -Wextra -o $(BUILD)/test_xlsxread tests/test_xlsxread.c src/xlsxread.c src/zipread.c src/inflate.c src/crc32.c src/csv.c src/units.c src/currency.c src/calc.c src/datecalc.c src/outstream.c src/records.c
	$(BUILD)/test_xlsxread docs/spikes/samples/fuellog-seek.xlsx docs/spikes/samples/sp6.xlsx
	mkdir -p $(BUILD)/out
	$(HOSTCC) -Wall -Wextra -o $(BUILD)/test_export tests/test_export.c $(EXPORT_SRCS)
	$(BUILD)/test_export
	python3 -I tools/check_xlsx.py $(BUILD)/out/fuellog-seek.xlsx $(BUILD)/out/fuellog-stream.xlsx
	$(HOSTCC) -Wall -Wextra -O2 -o $(BUILD)/test_inflate tests/test_inflate.c src/inflate.c src/zipread.c src/crc32.c
	python3 -I tools/check_inflate.py $(BUILD)/test_inflate $(BUILD)/inflate $(INFLATE_EXTRA)

check: $(APP).prc
	python3 -I tools/prcinfo.py $(APP).prc --expect-code 9

docker:
	docker run --rm --platform linux/amd64 -v "$(CURDIR)":/src fuellog-toolchain make

docker-image:
	docker build --platform linux/amd64 -t fuellog-toolchain toolchain

clean:
	rm -rf $(BUILD) $(APP).prc

.PHONY: all test check docker docker-image clean
