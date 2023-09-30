BUILD_DIR = obj_dir
VINC = /usr/share/verilator/include
ROOT = sys_array

HARDWARE_FILES = hardware/sys_array.v
SRC_DIR = software/src/
SRC_FILES = software/src/matrix_state.cpp
TEST_DIR = software/test/
TEST_FILES = software/test/utils.cpp software/test/sim_driver.cpp
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
	g++ -g -I$(VINC) -I$(BUILD_DIR)/ -I$(SRC_DIR) -I$(TEST_DIR) \
	$(VINC)/verilated.cpp $(VINC)/verilated_vcd_c.cpp \
	$(SRC_FILES) $(TEST_FILES) $(BUILD_DIR)/V$(ROOT)__ALL.a \
	-DMESHROWS=$(MESHROWS) -DMESHCOLS=$(MESHCOLS) -DBITWIDTH=$(BITWIDTH) -DTILEROWS=$(TILEROWS) -DTILECOLS=$(TILECOLS) \
	-o $(OUTPUT_SIM_FILE)

test:
	for num_mats in 1 10 50; do \
		for height in 10 100 500; do \
			./$(OUTPUT_SIM_FILE) --num-mats $$num_mats --height $$height --identity && \
			./$(OUTPUT_SIM_FILE) --num-mats $$num_mats --height $$height --random && \
			./$(OUTPUT_SIM_FILE) --num-mats $$num_mats --height $$height --random --affine && \
			./$(OUTPUT_SIM_FILE) --num-mats $$num_mats --height $$height --random --affine --negative; \
		done \
		; \
	done

clean:
	rm -rf $(BUILD_DIR)
	rm $(OUTPUT_SIM_FILE)
