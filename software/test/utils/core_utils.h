#include "Vuart.h"
#include "Vcore.h"
#include "Vcore_core.h"

#include "verilated.h"
#include "verilated_vcd_c.h"

// verilator build dependencies used for debugging mem state
#include "Vcore_imem__A100_B20.h"
#include "Vcore_blockmem__A100_B20_M2_T2.h"

#ifndef IMEM_ADDRSIZE
#define IMEM_ADDRSIZE 1 << 8
#endif

#ifndef BMEM_ADDRSIZE
#define BMEM_ADDRSIZE 1 << 16
#endif

#ifndef MESHUNITS
#define MESHUNITS 4
#endif

#ifndef TILEUNITS
#define TILEUNITS 4
#endif

// loader codes
#define INVALID 0x2A
#define IMEM 0x40
#define BMEM 0x80

// generates update code for threads 0-2
#define UPDATE(T0, T1, T2) 0xC0 | (T0) | (T1 << 1) | (T2 << 2)

void init(int& tickcount, Vcore* tb, VerilatedVcdC* tfp);
int imem_store(int& driver_tickcount, Vuart* driver_uart, VerilatedVcdC* driver_tfp, int& core_tickcount, Vcore* core, VerilatedVcdC* tfp, 
                std::array<bool, 3> curr_imem, int write_imem, unsigned int imem_addr, unsigned int imem_data);
int block_store(int& driver_tickcount, Vuart* driver_uart, VerilatedVcdC* driver_tfp, int& core_tickcount, Vcore* core, VerilatedVcdC* tfp, 
                unsigned int bmem_addr, std::array<int, MESHUNITS * MESHUNITS * TILEUNITS * TILEUNITS> bmem_data);
