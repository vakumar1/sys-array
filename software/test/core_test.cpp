#include "utils/test_utils.h"
#include "utils/core_utils.h"
#include "utils/uart_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include "Vcore.h"
#include "Vcore_core.h"
#include "Vuart.h"
#include "verilated.h"
#include "verilated_vcd_c.h"

#include <vector>
#include <string>

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Vcore* core = new Vcore;
    Vuart* driver_uart = new Vuart;
    Verilated::traceEverOn(true);
    
    int core_tickcount = 0;
    VerilatedVcdC* tfp = new VerilatedVcdC;
    core->trace(tfp, 99);
    tfp->open("core.vcd");
    init(core_tickcount, core, tfp);

    int driver_tickcount = 0;
    VerilatedVcdC* driver_tfp = new VerilatedVcdC;
    driver_uart->trace(driver_tfp, 99);
    driver_tfp->open("driver_uart.vcd");
    sender_init(driver_tickcount, driver_uart, driver_tfp);
    
    test_runner("[THREAD]", "WRITES/LOADS/COMPS + TERM", 
        [&core, &tfp, &core_tickcount, &driver_uart, &driver_tfp, &driver_tickcount](){
            std::array<bool, 3> curr_state = {0};
            unsigned int imem_addr = 0x12345678;
            unsigned int imem_data = 0xDEADBEEF;
            imem_store(driver_tickcount, driver_uart, driver_tfp, core_tickcount, core, tfp, curr_state, 0, imem_addr, imem_data);

            unsigned int bmem_addr = 0x87654321;
            std::array<int, MESHUNITS * MESHUNITS * TILEUNITS * TILEUNITS> bmem_data;
            for (int i = 0; i < MESHUNITS * MESHUNITS * TILEUNITS * TILEUNITS; i++) {
                bmem_data[i] = i;
            }
            block_store(driver_tickcount, driver_uart, driver_tfp, core_tickcount, core, tfp, bmem_addr, bmem_data);
        },
        [&tfp, &driver_tfp](){
            tfp->close();
            driver_tfp->close();
        });
    
    tfp->close();
    driver_tfp->close();
    printf("All core tests succeeded.\n");
    return 0;
}
