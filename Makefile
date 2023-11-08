BUILD_DIR = obj_dir
VINC = /usr/share/verilator/include
SRC_DIR = software/src/
TEST_DIR = software/test/

## TEST PARAMS

# sys array tests
ARRAY_TST_NAME = sys_array
ARRAY_HARDWARE_FILES = hardware/sys_array.v
ARRAY_SRC_FILES = software/src/matrix_state.cpp
ARRAY_TEST_FILES = software/test/sysarray_test.cpp
ARRAY_SIM_FILE = sysarray_simulation

# sys array controller tests
ARR_CTRL_TST_NAME = sys_array_controller
ARR_CTRL_HARDWARE_FILES = hardware/sys_array_controller.v $(ARRAY_HARDWARE_FILES)
ARR_CTRL_SRC_FILES = 
ARR_CTRL_TEST_FILES = software/test/sysarray_ctrl_test.cpp
ARR_CTRL_SIM_FILE = $(ARR_CTRL_TST_NAME)_simulation
ADDRWIDTH = 32

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

# uart controller tests
UART_CTRL_TST_NAME = uart_controller
UART_CTRL_HARDWARE_FILES = hardware/comms/uart_controller.v $(UART_HARDWARE_FILES) $(FIFO_HARDWARE_FILES)
UART_CTRL_SRC_FILES = 
UART_CTRL_TEST_FILES = software/test/uart_ctrl_test.cpp
UART_CTRL_SIM_FILE = uart_controller_simulation

# core tests
CORE_TST_NAME = core
CORE_HARDWARE_FILES = hardware/core.v hardware/memory/blockmem.v hardware/memory/imem.v $(ARR_CTRL_HARDWARE_FILES) $(UART_CTRL_HARDWARE_FILES)
CORE_SRC_FILES =
CORE_TEST_FILES = 
CORE_SIM_FILE = core_simulation

IMEM_ADDR_SIZE = 256 # 1 << 8
BMEM_ADDR_SIZE = 65536 # 1 << 16

## TEST TARGETS

all: veri sim

veri: veri-array veri-uart

sim: sim-array sim-uart

uart: veri-uart sim-uart

uartctrl: veri-uartctrl sim-uartctrl

fifo: veri-fifo sim-fifo

array: veri-array sim-array

arrayctrl: veri-arrayctrl sim-arrayctrl

core: veri-core

# BUILD VERILATOR
veri-uart:
	verilator -Wno-style \
	--trace -cc $(UART_HARDWARE_FILES)
	cd $(BUILD_DIR); \
	make -f V$(UART_TST_NAME).mk;

veri-uartctrl:
	verilator -Wno-style \
	--trace --trace-depth 25 -cc $(UART_CTRL_HARDWARE_FILES)
	cd $(BUILD_DIR); \
	make -f V$(UART_CTRL_TST_NAME).mk;

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

veri-core:
	verilator -Wno-style \
	-GBITWIDTH=$(BITWIDTH) -GIMEM_ADDRSIZE=$(IMEM_ADDR_SIZE) -GBMEM_ADDRSIZE=$(BMEM_ADDR_SIZE) -GMESHUNITS=$(MESHROWS) -GTILEUNITS=$(TILEROWS) \
	--trace --trace-max-width 1024 $(VINC)/verilated_fst_c.cpp -cc $(CORE_HARDWARE_FILES)
	cd $(BUILD_DIR); \
	make -f V$(CORE_TST_NAME).mk;

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

sim-uartctrl:
	g++ -g -I$(VINC) -I$(BUILD_DIR)/ -I$(SRC_DIR) -I$(TEST_DIR) \
	$(VINC)/verilated.cpp $(VINC)/verilated_vcd_c.cpp \
	$(UART_CTRL_SRC_FILES) $(UART_CTRL_TEST_FILES) $(BUILD_DIR)/V$(UART_CTRL_TST_NAME)__ALL.a \
	-o $(UART_CTRL_SIM_FILE)

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

test-uartctrl:
	./$(UART_CTRL_SIM_FILE)

test-fifo:
	./$(FIFO_SIM_FILE)

clean:
	- rm -rf $(BUILD_DIR)
	- rm *_simulation
	- rm *.vcd
