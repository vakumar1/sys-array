BUILD_DIR = obj_dir
VINC = /usr/share/verilator/include
SRC_DIR = software/src/
TEST_DIR = software/test/

# sys array tests
ARRAY_TST_NAME = sys_array
ARRAY_HARDWARE_FILES = hardware/sys_array.v
ARRAY_SRC_FILES = software/src/matrix_state.cpp
ARRAY_TEST_FILES = software/test/sysarray_test.cpp
ARRAY_SIM_FILE = sysarray_simulation

BITWIDTH = 32
MESHROWS = 4
MESHCOLS = 4
TILEROWS = 4
TILECOLS = 4

# uart tests
UART_TST_NAME = uart
UART_HARDWARE_FILES = hardware/comms/uart.v hardware/comms/uart_tx.v hardware/comms/uart_rx.v
UART_SRC_FILES = 
UART_TEST_FILES = software/test/uart_test.cpp
UART_SIM_FILE = uart_simulation


all: veri sim

veri: veri-array veri-uart

veri-uart:
	verilator -Wno-style \
	--trace -cc $(UART_HARDWARE_FILES)
	cd $(BUILD_DIR); \
	make -f V$(UART_TST_NAME).mk;

veri-array:
	verilator -Wno-style \
	-GMESHROWS=$(MESHROWS) -GMESHCOLS=$(MESHCOLS) -GBITWIDTH=$(BITWIDTH) -GTILEROWS=$(TILEROWS) -GTILECOLS=$(TILECOLS) \
	--trace --trace-max-width 1024 -cc $(ARRAY_HARDWARE_FILES)
	cd $(BUILD_DIR); \
	make -f V$(ARRAY_TST_NAME).mk;

sim: sim-array sim-uart

sim-array:
	g++ -g -I$(VINC) -I$(BUILD_DIR)/ -I$(SRC_DIR) -I$(TEST_DIR) \
	$(VINC)/verilated.cpp $(VINC)/verilated_vcd_c.cpp \
	$(ARRAY_SRC_FILES) $(ARRAY_TEST_FILES) $(BUILD_DIR)/V$(ARRAY_TST_NAME)__ALL.a \
	-DMESHROWS=$(MESHROWS) -DMESHCOLS=$(MESHCOLS) -DBITWIDTH=$(BITWIDTH) -DTILEROWS=$(TILEROWS) -DTILECOLS=$(TILECOLS) \
	-o $(ARRAY_SIM_FILE)

sim-uart:
	g++ -g -I$(VINC) -I$(BUILD_DIR)/ -I$(SRC_DIR) -I$(TEST_DIR) \
	$(VINC)/verilated.cpp $(VINC)/verilated_vcd_c.cpp \
	$(UART_SRC_FILES) $(UART_TEST_FILES) $(BUILD_DIR)/V$(UART_TST_NAME)__ALL.a \
	-o $(UART_SIM_FILE)

test: test-array test-uart

test-array:
	for num_mats in 1 10 50; do \
		for height in 10 100 500; do \
			./$(ARRAY_SIM_FILE) --num-mats $$num_mats --height $$height --identity && \
			./$(ARRAY_SIM_FILE) --num-mats $$num_mats --height $$height --random && \
			./$(ARRAY_SIM_FILE) --num-mats $$num_mats --height $$height --random --affine && \
			./$(ARRAY_SIM_FILE) --num-mats $$num_mats --height $$height --random --affine --negative; \
		done \
		; \
	done

test-uart:
	./$(UART_SIM_FILE)

clean:
	rm -rf $(BUILD_DIR)
	rm $(ARRAY_SIM_FILE)
	rm $(UART_SIM_FILE)
