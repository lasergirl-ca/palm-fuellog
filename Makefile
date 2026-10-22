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

CFLAGS   := -O2 -Wall -palmos5r3 -Irsc -Isrc -MMD
BUILD    := build

SRCS := src/main.c src/hello2.c src/calc.c src/device.c src/graph.c src/vfstest.c \
        src/outstream.c src/xlsx.c src/pdf.c
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

EXPORT_SRCS := src/outstream.c src/xlsx.c src/pdf.c

test: $(BUILD)
	$(HOSTCC) -Wall -Wextra -o $(BUILD)/test_calc tests/test_calc.c src/calc.c
	$(BUILD)/test_calc
	mkdir -p $(BUILD)/out
	$(HOSTCC) -Wall -Wextra -o $(BUILD)/test_export tests/test_export.c $(EXPORT_SRCS)
	$(BUILD)/test_export
	python3 -I tools/check_xlsx.py $(BUILD)/out/fuellog-seek.xlsx $(BUILD)/out/fuellog-stream.xlsx

check: $(APP).prc
	python3 -I tools/prcinfo.py $(APP).prc --expect-code 4

docker:
	docker run --rm --platform linux/amd64 -v "$(CURDIR)":/src fuellog-toolchain make

docker-image:
	docker build --platform linux/amd64 -t fuellog-toolchain toolchain

clean:
	rm -rf $(BUILD) $(APP).prc

.PHONY: all test check docker docker-image clean
