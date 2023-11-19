#include "utils.h"
#include "uart_util.h"

#include <stdio.h>
#include <stdlib.h>
#include "Vcore.h"
#include "Vcore_core.h"
#include "Vcore_imem__A100_B20.h"
#include "Vuart.h"
#include "verilated.h"
#include "verilated_vcd_c.h"
#include <vector>
#include <string>

#define INVALID 0x2A
#define IMEM 0x40
#define BMEM 0x80
#define UPDATE(T0, T1, T2) 0xC0 | (T0) | (T1 << 1) | (T2 << 2)

void tick(int& tickcount, Vcore* tb, VerilatedVcdC* tfp) {
    tb->eval();
    if (tickcount > 0) {
        if (tfp)
            tfp->dump(tickcount * 10 - 2);
    }
    tb->clock = 1;
    tb->eval();
    if (tfp)
        tfp->dump(tickcount * 10);
    tb->clock = 0;
    tb->eval();
    if (tfp)
        tfp->dump(tickcount * 10 + 5);
    tickcount++;
}

void init(int& tickcount, Vcore* tb, VerilatedVcdC* tfp) {
    tb->reset = 1;
    tick(tickcount, tb, tfp);
    tb->reset = 0;

    // needed to overwrite default of serial=0 (which indicates TX start)
    tb->serial_in = 1;
    tick(tickcount, tb, tfp);
}

void send_byte(int& driver_tickcount, Vuart* driver_uart, VerilatedVcdC* driver_tfp, 
                int& core_tickcount, Vcore* core, VerilatedVcdC* tfp, char byte) {
    tick(core_tickcount, core, tfp);
    sender_tick(driver_uart, driver_tickcount, driver_tfp, byte, 1, 1);

    // send initial bit and initialize receiver to READING state
    for (int i = 0; i < SYMBOL_TICK_COUNT; i++) {
        core->serial_in = driver_uart->serial_out;
        tick(core_tickcount, core, tfp);
        sender_tick(driver_uart, driver_tickcount, driver_tfp, 0x0, 0, 1);
    }

    // send bits 0-7
    for (int bit_pos = 7; bit_pos >= 0; bit_pos--) {
        for (int i = 0; i < SYMBOL_TICK_COUNT; i++) {
            core->serial_in = driver_uart->serial_out;
            tick(core_tickcount, core, tfp);
            sender_tick(driver_uart, driver_tickcount, driver_tfp, 0x0, 0, 1);
        }
    }

    // send final bit to close connection
    for (int i = 0; i < SYMBOL_TICK_COUNT; i++) {
        core->serial_in = driver_uart->serial_out;
        tick(core_tickcount, core, tfp);
        sender_tick(driver_uart, driver_tickcount, driver_tfp, 0x0, 0, 1);
    }
}

int imem_store(int& driver_tickcount, Vuart* driver_uart, VerilatedVcdC* driver_tfp, int& core_tickcount, Vcore* core, VerilatedVcdC* tfp, 
                std::array<bool, 3> curr_imem, int write_imem, unsigned int imem_addr, unsigned int imem_data) {
    // send update to halt thread IMEM_IDX
    int run_thread0 = (write_imem == 0 || !curr_imem[0]) ? 0 : 1;
    int run_thread1 = (write_imem == 1 || !curr_imem[1]) ? 0 : 1;
    int run_thread2 = (write_imem == 2 || !curr_imem[2]) ? 0 : 1;
    send_byte(driver_tickcount, driver_uart, driver_tfp, core_tickcount, core, tfp, UPDATE(run_thread0, run_thread1, run_thread2));

    // send imem address and data
    send_byte(driver_tickcount, driver_uart, driver_tfp, core_tickcount, core, tfp, IMEM);
    for (int i = 0; i < 3; i++) {
        char byte = static_cast<char>((imem_addr >> (i * 8)) & (0xFF));
        send_byte(driver_tickcount, driver_uart, driver_tfp, core_tickcount, core, tfp, byte);
    }
    for (int i = 0; i < 3; i++) {
        char byte = static_cast<char>((imem_data >> (i * 8)) & (0xFF));
        send_byte(driver_tickcount, driver_uart, driver_tfp, core_tickcount, core, tfp, byte);
    }

    // check that imem was correctly stored
    Vcore_imem__A100_B20* imem = write_imem == 0 ? core->core->_imem0
                                : write_imem == 1 ? core->core->_imem1
                                : core->core->_imem2;
    unsigned int actual_imem_data = imem->instr_mem[(imem_addr >> 2) << 2];
    if (actual_imem_data != imem_data) {
        return SIM_ERROR;
    }
    return SUCCESS;
}

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
    
    std::array<bool, 3> curr_state = {0};
    unsigned int imem_addr = 0x2A;
    unsigned int imem_data = 0x2B;
    int res;
    res = imem_store(driver_tickcount, driver_uart, driver_tfp, core_tickcount, core, tfp, curr_state, 0, imem_addr, imem_data);
    if (res != SUCCESS) {
        printf("Imem store failed.\n");
        tfp->close();
        driver_tfp->close();
        return 0;
    }
    tfp->close();
    driver_tfp->close();
    printf("All core tests succeeded.\n");
    return 0;
}
