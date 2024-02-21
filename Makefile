BUILD_DIR = obj_dir
VINC = /usr/share/verilator/include
SVDPIINC = /usr/share/verilator/include/vltstd
SRC_DIR = software/src/
TEST_DIR = software/test/
SIM_COMPILE_CMD = g++ -g -I$(VINC) -I$(SVDPIINC) -I$(BUILD_DIR)/ -I$(SRC_DIR) -I$(TEST_DIR) \
				$(VINC)/verilated.cpp $(VINC)/verilated_vcd_c.cpp \


## PARAMS

# VERILATOR BUILD RESULT NAMES
FIFO_VERI_FILES = $(BUILD_DIR)/Vfifo__ALL.a
UART_VERI_FILES = $(BUILD_DIR)/Vuart__ALL.a
UART_CTRL_VERI_FILES = $(BUILD_DIR)/Vuart_controller__ALL.a
ARR_VERI_FILES = $(BUILD_DIR)/Vsys_array__ALL.a
ARR_CTRL_VERI_FILES = $(BUILD_DIR)/Vsys_array_controller__ALL.a
THREAD_VERI_FILES = $(BUILD_DIR)/Vthread__ALL.a
CORE_VERI_FILES = $(BUILD_DIR)/Vcore__ALL.a

# SIMULATION BUILD RESULT PARAMS (must include source files, dependencies, and verilator build files)
# utils
UTIL_SRC_FILES = software/test/uart_util.cpp software/test/utils.cpp $(UART_VERI_FILES)

# fifo tests
FIFO_HARDWARE_FILES = hardware/comms/fifo.v
FIFO_SRC_FILES = software/test/fifo_test.cpp $(FIFO_VERI_FILES)
FIFO_SIM_FILE = fifo_simulation

# uart tests
UART_HARDWARE_FILES = hardware/comms/uart.v hardware/comms/uart_tx.v hardware/comms/uart_rx.v
UART_SRC_FILES = software/test/uart_test.cpp $(UTIL_SRC_FILES) $(UART_VERI_FILES)
UART_SIM_FILE = uart_simulation

# uart controller tests
UART_CTRL_HARDWARE_FILES = hardware/comms/uart_controller.v $(UART_HARDWARE_FILES) $(FIFO_HARDWARE_FILES)
UART_CTRL_SRC_FILES = software/test/uart_ctrl_test.cpp $(UTIL_SRC_FILES) $(UART_CTRL_VERI_FILES) $(UART_VERI_FILES)
UART_CTRL_SIM_FILE = uart_controller_simulation

# sys array tests
ARRAY_HARDWARE_FILES = hardware/sys_array.v
ARRAY_SRC_FILES = software/src/matrix_state.cpp software/test/sysarray_test.cpp $(UTIL_SRC_FILES) $(ARR_VERI_FILES)
ARRAY_SIM_FILE = sysarray_simulation

# sys array controller tests
ARR_CTRL_HARDWARE_FILES = hardware/sys_array_controller.v $(ARRAY_HARDWARE_FILES)
ARR_CTRL_SRC_FILES = software/test/sysarray_ctrl_test.cpp $(UTIL_SRC_FILES) $(ARR_CTRL_VERI_FILES)
ARR_CTRL_SIM_FILE = sysarray_controller_simulation
ADDRWIDTH = 32
BITWIDTH = 32
MESHROWS = 2
MESHCOLS = 2
TILEROWS = 2
TILECOLS = 2

# thread tests
THREAD_HARDWARE_FILES = hardware/thread.v
THREAD_SRC_FILES = software/test/thread_test.cpp $(UTIL_SRC_FILES) $(UART_VERI_FILES) $(THREAD_VERI_FILES)
THREAD_SIM_FILE = thread_simulation

# core tests
CORE_HARDWARE_FILES = hardware/core.v hardware/thread.v hardware/memory/blockmem.v hardware/memory/imem.v $(ARR_CTRL_HARDWARE_FILES) $(UART_CTRL_HARDWARE_FILES)
CORE_SRC_FILES = software/test/core_test.cpp $(UTIL_SRC_FILES) $(CORE_VERI_FILES)
CORE_SIM_FILE = core_simulation
IMEM_ADDR_SIZE = 256 # 1 << 8
BMEM_ADDR_SIZE = 256 # 1 << 16

## TARGETS

fifo: veri-fifo sim-fifo

uart: veri-uart sim-uart

uartctrl: veri-uartctrl sim-uartctrl

array: veri-array sim-array

arrayctrl: veri-arrayctrl sim-arrayctrl

thread: veri-thread sim-thread

core: veri-core sim-core

# BUILD VERILATOR
# verilator build dependencies required for building the simulation should be added as build targets
# e.g., build veri-uart when the simulation uses the utils target
veri-fifo:
	verilator -Wno-style \
	--trace -cc $(FIFO_HARDWARE_FILES)
	cd $(BUILD_DIR); \
	make -f Vfifo.mk;

veri-uart:
	verilator -Wno-style \
	--trace -cc $(UART_HARDWARE_FILES)
	cd $(BUILD_DIR); \
	make -f Vuart.mk;

veri-uartctrl: veri-uart
	verilator -Wno-style \
	--trace --trace-depth 25 -cc $(UART_CTRL_HARDWARE_FILES)
	cd $(BUILD_DIR); \
	make -f Vuart_controller.mk;

veri-array:
	verilator -Wno-style \
	-GMESHROWS=$(MESHROWS) -GMESHCOLS=$(MESHCOLS) -GBITWIDTH=$(BITWIDTH) -GTILEROWS=$(TILEROWS) -GTILECOLS=$(TILECOLS) \
	--trace --trace-max-width 1024 -cc $(ARRAY_HARDWARE_FILES)
	cd $(BUILD_DIR); \
	make -f Vsys_array.mk;

veri-arrayctrl:
	verilator -Wno-style \
	-GMESHUNITS=$(MESHROWS) -GTILEUNITS=$(TILEROWS) -GBITWIDTH=$(BITWIDTH) \
	--trace --trace-max-width 1024 $(VINC)/verilated_fst_c.cpp -cc $(ARR_CTRL_HARDWARE_FILES)
	cd $(BUILD_DIR); \
	make -f Vsys_array_controller.mk;

veri-thread:
	verilator -Wno-style \
	-GBITWIDTH=$(BITWIDTH) -GMESHUNITS=$(MESHROWS) -GTILEUNITS=$(TILEROWS) \
	--trace -cc $(THREAD_HARDWARE_FILES)
	cd $(BUILD_DIR); \
	make -f Vthread.mk;

veri-core: veri-uart
	verilator -Wno-style \
	-GBITWIDTH=$(BITWIDTH) -GIMEM_ADDRSIZE=$(IMEM_ADDR_SIZE) -GBMEM_ADDRSIZE=$(BMEM_ADDR_SIZE) -GMESHUNITS=$(MESHROWS) -GTILEUNITS=$(TILEROWS) \
	--trace --trace-max-width 1024 $(VINC)/verilated_fst_c.cpp -cc $(CORE_HARDWARE_FILES)
	cd $(BUILD_DIR); \
	make -f Vcore.mk;

# BUILD SIMULATION
sim-fifo:
	$(SIM_COMPILE_CMD) \
	$(FIFO_SRC_FILES) \
	-o $(FIFO_SIM_FILE)

sim-uart:
	$(SIM_COMPILE_CMD) \
	$(UART_SRC_FILES) \
	-o $(UART_SIM_FILE)

sim-uartctrl:
	$(SIM_COMPILE_CMD) \
	$(UART_CTRL_SRC_FILES) \
	-o $(UART_CTRL_SIM_FILE)

sim-array:
	$(SIM_COMPILE_CMD) \
	$(ARRAY_SRC_FILES) \
	-DMESHROWS=$(MESHROWS) -DMESHCOLS=$(MESHCOLS) -DBITWIDTH=$(BITWIDTH) -DTILEROWS=$(TILEROWS) -DTILECOLS=$(TILECOLS) \
	-o $(ARRAY_SIM_FILE)

sim-arrayctrl:
	$(SIM_COMPILE_CMD) \
	$(ARR_CTRL_SRC_FILES) \
	-DBITWIDTH=$(BITWIDTH) -DMESHUNITS=$(MESHROWS) -DTILEUNITS=$(TILEROWS) \
	-o $(ARR_CTRL_SIM_FILE)

sim-thread:
	$(SIM_COMPILE_CMD) \
	$(THREAD_SRC_FILES) \
	-DBITWIDTH=$(BITWIDTH) -DMESHUNITS=$(MESHROWS) -DTILEUNITS=$(TILEROWS) \
	-o $(THREAD_SIM_FILE)

sim-core:
	$(SIM_COMPILE_CMD) \
	$(CORE_SRC_FILES) \
	-DIMEM_ADDRSIZE=$(IMEM_ADDR_SIZE) -DBMEM_ADDRSIZE=$(BMEM_ADDR_SIZE) -DMESHUNITS=$(MESHROWS) -DTILEUNITS=$(TILEROWS) \
	-o $(CORE_SIM_FILE)


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
