sim_file = simulation
build_dir = obj_dir
VINC = /usr/share/verilator/include
root = sys_array

MESHROWS = 16
MESHCOLUMNS = 16
BITWIDTH = 32
TILEROWS = 16
TILECOLUMNS = 16

all: veri sim

sim:
	g++ -I$(VINC) -I$(build_dir)/ $(VINC)/verilated.cpp $(VINC)/verilated_vcd_c.cpp sim_driver.cpp $(build_dir)/V$(root)__ALL.a -o $(sim_file)

veri:
	verilator -Wall \
	-GMESHROWS=$(MESHROWS) -GMESHCOLUMNS=$(MESHCOLUMNS) -GBITWIDTH=$(BITWIDTH) -GTILEROWS=$(TILEROWS) -GTILECOLUMNS=$(TILECOLUMNS) \
	--trace -cc $(root).v
	cd $(build_dir); \
	make -f V$(root).mk;

clean:
	rm -rf $(build_dir)
	rm $(sim_file)
