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
            std::array<bool, 4> init_update = { 0 };
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
                bmem_data[i] = i;
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

    test_runner("[CORE]", "IMEM/BMEM STORE + LOAD + COMP + WRITE", 
        [&core, &tfp, &core_tickcount, &driver_uart, &driver_tfp, &driver_tickcount](){
            std::array<bool, 4> update;

            // halt all threads
            update = { 0 };
            thread_update(driver_tickcount, driver_uart, driver_tfp, core_tickcount, core, tfp, update);

            // write B bmem data to 0x0200
            unsigned int B_addr = 0x00000200;
            std::array<int, MESHUNITS * MESHUNITS * TILEUNITS * TILEUNITS> B_data;
            for (int i = 0; i < MESHUNITS * MESHUNITS * TILEUNITS * TILEUNITS; i++) {
                B_data[i] = i;
            }
            block_store(driver_tickcount, driver_uart, driver_tfp, core_tickcount, core, tfp, B_addr, B_data);

            // write A bmem data to 0x0400
            unsigned int A_addr = 0x00000400;
            std::array<int, MESHUNITS * MESHUNITS * TILEUNITS * TILEUNITS> A_data;
            for (int i = 0; i < MESHUNITS * MESHUNITS * TILEUNITS * TILEUNITS; i++) {
                A_data[i] = i;
            }
            block_store(driver_tickcount, driver_uart, driver_tfp, core_tickcount, core, tfp, A_addr, A_data);

            // write D bmem data to 0x0600
            unsigned int D_addr = 0x00000600;
            std::array<int, MESHUNITS * MESHUNITS * TILEUNITS * TILEUNITS> D_data;
            for (int i = 0; i < MESHUNITS * MESHUNITS * TILEUNITS * TILEUNITS; i++) {
                D_data[i] = 0;
            }
            block_store(driver_tickcount, driver_uart, driver_tfp, core_tickcount, core, tfp, D_addr, D_data);

            // send valid imem data (load + comp + write + term)
            // load B1
            load_instr_t l = { (unsigned char) ((B_addr >> 8) & 0xFF) };

            // comp C = B * A + D
            unsigned int C_addr = 0x00000800;
            comp_instr_t c = { (unsigned char) ((A_addr >> 8) & 0xFF), 
                                (unsigned char) ((D_addr >> 8) & 0xFF),
                                (unsigned char) ((C_addr >> 8) & 0xFF)};

            // write B4
            unsigned char header = 0x2A;
            write_instr_t w = { header, (unsigned char) ((C_addr >> 8) & 0xFF) };

            term_instr_t t = {};
            imem_store(driver_tickcount, driver_uart, driver_tfp, core_tickcount, core, tfp, 0, 0x0, load_instr_to_bits(l));
            imem_store(driver_tickcount, driver_uart, driver_tfp, core_tickcount, core, tfp, 0, 0x4, comp_instr_to_bits(c));
            imem_store(driver_tickcount, driver_uart, driver_tfp, core_tickcount, core, tfp, 0, 0x8, write_instr_to_bits(w));
            imem_store(driver_tickcount, driver_uart, driver_tfp, core_tickcount, core, tfp, 0, 0xC, term_instr_to_bits(t));

            // enable and start thread 0;
            update = {1, 1, 0, 0};
            thread_update(driver_tickcount, driver_uart, driver_tfp, core_tickcount, core, tfp, update);

            // read bmem bytes from UART
            std::array<int, MESHUNITS * MESHUNITS * TILEUNITS * TILEUNITS> expected_C_data;
            for (int i = 0; i < MESHUNITS * TILEUNITS; i++) {
                for (int j = 0; j < MESHUNITS * TILEUNITS; j++) {
                    expected_C_data[i * (MESHUNITS * TILEUNITS) + j] = D_data[i * (MESHUNITS * TILEUNITS) + j];
                    for (int k = 0; k < MESHUNITS * TILEUNITS; k++) {
                        expected_C_data[i * (MESHUNITS * TILEUNITS) + j] += 
                            A_data[i * (MESHUNITS * TILEUNITS) + k] * B_data[k * (MESHUNITS * TILEUNITS) + j];
                    }
                }
            }
            read_bmem(driver_tickcount, driver_uart, driver_tfp, core_tickcount, core, tfp, C_addr, header, expected_C_data);
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
