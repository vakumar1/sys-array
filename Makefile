SIM_FILE = simulation.o
BUILD_DIR = obj_dir
VINC = /usr/share/verilator/include
ROOT = sys_array
INPUT_FILES = sys_array.v

BITWIDTH = 32
MESHROWS = 4
MESHCOLS = 4
TILEROWS = 2
TILECOLS = 2

all: veri sim

sim:
	g++ -g -I$(VINC) -I$(BUILD_DIR)/ $(VINC)/verilated.cpp $(VINC)/verilated_vcd_c.cpp sim_driver.cpp $(BUILD_DIR)/V$(ROOT)__ALL.a \
	-DMESHROWS=$(MESHROWS) -DMESHCOLS=$(MESHCOLS) -DBITWIDTH=$(BITWIDTH) -DTILEROWS=$(TILEROWS) -DTILECOLS=$(TILECOLS) \
	-o $(SIM_FILE)

veri:
	verilator -Wno-style \
	-GMESHROWS=$(MESHROWS) -GMESHCOLS=$(MESHCOLS) -GBITWIDTH=$(BITWIDTH) -GTILEROWS=$(TILEROWS) -GTILECOLS=$(TILECOLS) \
	--trace -cc $(INPUT_FILES)
	cd $(BUILD_DIR); \
	make -f V$(ROOT).mk;

clean:
	rm -rf $(BUILD_DIR)
	rm $(SIM_FILE)
