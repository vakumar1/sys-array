BUILD_DIR = obj_dir
VINC = /usr/share/verilator/include
ROOT = sys_array

HARDWARE_FILES = hardware/sys_array.v
SRC_DIR = software/src/
SRC_FILES = software/src/matrix_state.cpp 
UTILS_FILES = software/src/utils.cpp 
OUTPUT_SRC_FILE = src.o
OUTPUT_UTILS_FILE = utils.o
TEST_FILES = software/test/sim_driver.cpp
OUTPUT_SIM_FILE = simulation

BITWIDTH = 32
MESHROWS = 4
MESHCOLS = 4
TILEROWS = 4
TILECOLS = 4

all: veri sim

veri:
	verilator -Wno-style \
	-GMESHROWS=$(MESHROWS) -GMESHCOLS=$(MESHCOLS) -GBITWIDTH=$(BITWIDTH) -GTILEROWS=$(TILEROWS) -GTILECOLS=$(TILECOLS) \
	--trace --trace-max-width 1024 -cc $(HARDWARE_FILES)
	cd $(BUILD_DIR); \
	make -f V$(ROOT).mk;

sim:
	g++ -g -I$(VINC) -I$(BUILD_DIR)/ -I$(SRC_DIR) \
	$(VINC)/verilated.cpp $(VINC)/verilated_vcd_c.cpp \
	$(UTILS_FILES) $(SRC_FILES) $(TEST_FILES) $(BUILD_DIR)/V$(ROOT)__ALL.a \
	-DMESHROWS=$(MESHROWS) -DMESHCOLS=$(MESHCOLS) -DBITWIDTH=$(BITWIDTH) -DTILEROWS=$(TILEROWS) -DTILECOLS=$(TILECOLS) \
	-o $(OUTPUT_SIM_FILE)

test:
	./$(OUTPUT_SIM_FILE)

clean:
	rm -rf $(BUILD_DIR)
	rm $(OUTPUT_SIM_FILE)
