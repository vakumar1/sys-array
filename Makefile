SIM_FILE = simulation
BUILD_DIR = obj_dir
VINC = /usr/share/verilator/include
ROOT = sys_array
INPUT_FILES = sys_array.v

BITWIDTH = 32
MESHROWS = 4
MESHCOLUMNS = 4
TILEROWS = 4
TILECOLUMNS = 4

all: veri sim

sim:
	g++ -g -I$(VINC) -I$(BUILD_DIR)/ $(VINC)/verilated.cpp $(VINC)/verilated_vcd_c.cpp sim_driver.cpp $(BUILD_DIR)/V$(ROOT)__ALL.a -o $(SIM_FILE)

veri:
	verilator -Wno-style \
	-GMESHROWS=$(MESHROWS) -GMESHCOLUMNS=$(MESHCOLUMNS) -GBITWIDTH=$(BITWIDTH) -GTILEROWS=$(TILEROWS) -GTILECOLUMNS=$(TILECOLUMNS) \
	--trace -cc $(INPUT_FILES)
	cd $(BUILD_DIR); \
	make -f V$(ROOT).mk;

clean:
	rm -rf $(BUILD_DIR)
	rm $(SIM_FILE)
