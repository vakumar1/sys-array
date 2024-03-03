#include "utils/test_utils.h"
#include "utils/core_utils.h"
#include "utils/uart_utils.h"
#include "utils/instr_utils.h"

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
    core->trace(tfp, 500);
    tfp->open("core.vcd");
    init(core_tickcount, core, tfp);

    int driver_tickcount = 0;
    VerilatedVcdC* driver_tfp = new VerilatedVcdC;
    driver_uart->trace(driver_tfp, 99);
    driver_tfp->open("driver_uart.vcd");
    sender_init(driver_tickcount, driver_uart, driver_tfp);
    
    test_runner("[CORE]", "IMEM/BMEM STORE", 
        [&core, &tfp, &core_tickcount, &driver_uart, &driver_tfp, &driver_tickcount](){
            // halt all threads
            std::array<bool, 3> init_update = {0};
            thread_update(driver_tickcount, driver_uart, driver_tfp, core_tickcount, core, tfp, init_update);

            // send random imem data
            unsigned int imem_addr = 0x12345678;
            unsigned int imem_data = 0xDEADBEEF;
            imem_store(driver_tickcount, driver_uart, driver_tfp, core_tickcount, core, tfp, 0, imem_addr, imem_data);

            // send random bmem data
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

    test_runner("[CORE]", "IMEM/BMEM STORE + WRITES", 
        [&core, &tfp, &core_tickcount, &driver_uart, &driver_tfp, &driver_tickcount](){
            std::array<bool, 4> update;


            // halt all threads
            update = { 0 };
            thread_update(driver_tickcount, driver_uart, driver_tfp, core_tickcount, core, tfp, update);

            // write bmem data to 0x0800
            unsigned char header = 0x2A;
            unsigned int bmem_addr = 0x00000800;
            std::array<int, MESHUNITS * MESHUNITS * TILEUNITS * TILEUNITS> bmem_data;
            for (int i = 0; i < MESHUNITS * MESHUNITS * TILEUNITS * TILEUNITS; i++) {
                bmem_data[i] = i; //(i << (8 * 3)) | (i << (8 * 2)) | (i << (8 * 1)) | (i << (8 * 0));
            }
            block_store(driver_tickcount, driver_uart, driver_tfp, core_tickcount, core, tfp, bmem_addr, bmem_data);

            // send valid imem data (write + term)
            write_instr_t w = { header, (unsigned char) ((bmem_addr >> 8) & 0xFF) };
            imem_store(driver_tickcount, driver_uart, driver_tfp, core_tickcount, core, tfp, 0, 0x0, write_instr_to_bits(w));

            term_instr_t t = {};
            imem_store(driver_tickcount, driver_uart, driver_tfp, core_tickcount, core, tfp, 0, 0x4, term_instr_to_bits(t));

            // enable and start thread 0;
            update = {1, 1, 0, 0};
            thread_update(driver_tickcount, driver_uart, driver_tfp, core_tickcount, core, tfp, update);

            // read bmem bytes from UART
            read_bmem(driver_tickcount, driver_uart, driver_tfp, core_tickcount, core, tfp, bmem_addr, header, bmem_data);

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
