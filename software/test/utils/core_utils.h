#include "Vuart.h"
#include "Vcore.h"
#include "Vcore_core.h"

#include "verilated.h"
#include "verilated_vcd_c.h"

// verilator build dependencies used for debugging mem state
#include "Vcore_imem__A100_B20.h"
#include "Vcore_blockmem__A10000_B20_M2_T2.h"

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
#define UPDATE 0xC0

// generates update code for threads 0-1
#define UPDATE_BYTE(T0_start, T0_enabled, T1_start, T1_enabled) UPDATE | (T0_start) | (T0_enabled << 1) | (T1_start << 2) | (T1_enabled << 3)

void core_tick(int& tickcount, Vcore* tb, VerilatedVcdC* tfp, int serial_in);
void init(int& tickcount, Vcore* tb, VerilatedVcdC* tfp);
int imem_store(int& driver_tickcount, Vuart* driver_uart, VerilatedVcdC* driver_tfp, int& core_tickcount, Vcore* core, VerilatedVcdC* tfp, 
                int write_imem, unsigned int imem_addr, unsigned int imem_data);
int block_store(int& driver_tickcount, Vuart* driver_uart, VerilatedVcdC* driver_tfp, int& core_tickcount, Vcore* core, VerilatedVcdC* tfp, 
                unsigned int bmem_addr, std::array<int, MESHUNITS * MESHUNITS * TILEUNITS * TILEUNITS>& bmem_data);
int thread_update(int& driver_tickcount, Vuart* driver_uart, VerilatedVcdC* driver_tfp, int& core_tickcount, Vcore* core, VerilatedVcdC* tfp, 
                std::array<bool, 4> update_state);
int read_bmem(int& driver_tickcount, Vuart* driver_uart, VerilatedVcdC* driver_tfp,
                int& core_tickcount, Vcore* core, VerilatedVcdC* tfp, unsigned int bmem_addr,
                unsigned char header, std::array<int, MESHUNITS * MESHUNITS * TILEUNITS * TILEUNITS>& bmem_data);
