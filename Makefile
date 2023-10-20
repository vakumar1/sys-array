BUILD_DIR = obj_dir
VINC = /usr/share/verilator/include
SRC_DIR = software/src/
TEST_DIR = software/test/

# sys array controller tests
ARR_CTRL_TST_NAME = sys_array_controller
ARR_CTRL_HARDWARE_FILES = hardware/sys_array_controller.v hardware/sys_array.v
ARR_CTRL_SRC_FILES = 
ARR_CTRL_TEST_FILES = software/test/sysarray_ctrl_test.cpp
ARR_CTRL_SIM_FILE = $(ARR_CTRL_TST_NAME)_simulation
ADDRWIDTH = 32

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

# fifo tests
FIFO_TST_NAME = fifo
FIFO_HARDWARE_FILES = hardware/comms/fifo.v
FIFO_SRC_FILES = 
FIFO_TEST_FILES = software/test/fifo_test.cpp
FIFO_SIM_FILE = fifo_simulation


all: veri sim

veri: veri-array veri-uart

sim: sim-array sim-uart

uart: veri-uart sim-uart

fifo: veri-fifo sim-fifo

sys_array: veri-array sim-array

sys_array_ctrl: veri-arrayctrl sim-arrayctrl

# BUILD VERILATOR
veri-uart:
	verilator -Wno-style \
	--trace -cc $(UART_HARDWARE_FILES)
	cd $(BUILD_DIR); \
	make -f V$(UART_TST_NAME).mk;

veri-fifo:
	verilator -Wno-style \
	--trace -cc $(FIFO_HARDWARE_FILES)
	cd $(BUILD_DIR); \
	make -f V$(FIFO_TST_NAME).mk;

veri-array:
	verilator -Wno-style \
	-GMESHROWS=$(MESHROWS) -GMESHCOLS=$(MESHCOLS) -GBITWIDTH=$(BITWIDTH) -GTILEROWS=$(TILEROWS) -GTILECOLS=$(TILECOLS) \
	--trace --trace-max-width 1024 -cc $(ARRAY_HARDWARE_FILES)
	cd $(BUILD_DIR); \
	make -f V$(ARRAY_TST_NAME).mk;

veri-arrayctrl:
	verilator -Wno-style \
	-GMESHUNITS=$(MESHROWS) -GTILEUNITS=$(TILEROWS) -GBITWIDTH=$(BITWIDTH) \
	--trace --trace-max-width 1024 $(VINC)/verilated_fst_c.cpp -cc $(ARR_CTRL_HARDWARE_FILES)
	cd $(BUILD_DIR); \
	make -f V$(ARR_CTRL_TST_NAME).mk;

# BUILD SIMULATION
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

sim-fifo:
	g++ -g -I$(VINC) -I$(BUILD_DIR)/ -I$(SRC_DIR) -I$(TEST_DIR) \
	$(VINC)/verilated.cpp $(VINC)/verilated_vcd_c.cpp \
	$(FIFO_SRC_FILES) $(FIFO_TEST_FILES) $(BUILD_DIR)/V$(FIFO_TST_NAME)__ALL.a \
	-o $(FIFO_SIM_FILE)

sim-arrayctrl:
	g++ -g -I$(VINC) -I$(BUILD_DIR)/ -I$(SRC_DIR) -I$(TEST_DIR) \
	$(VINC)/verilated.cpp $(VINC)/verilated_vcd_c.cpp \
	$(ARR_CTRL_SRC_FILES) $(ARR_CTRL_TEST_FILES) $(BUILD_DIR)/V$(ARR_CTRL_TST_NAME)__ALL.a \
	-DBITWIDTH=$(BITWIDTH) -DMESHUNITS=$(MESHROWS) -DTILEUNITS=$(TILEROWS) \
	-o $(ARR_CTRL_SIM_FILE)

# RUN TESTS
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

test-fifo:
	./$(FIFO_SIM_FILE)

clean:
	- rm -rf $(BUILD_DIR)
	- rm $(ARRAY_SIM_FILE)
	- rm $(UART_SIM_FILE)
	- rm $(FIFO_SIM_FILE)
	- rm $(ARR_CTRL_SIM_FILE)
	- rm *.vcd
