#include "utils.h"
#include "uart_util.h"

#include <stdio.h>
#include <stdlib.h>
#include "Vuart_controller.h"
#include "verilated.h"
#include "verilated_vcd_c.h"
#include <vector>
#include <string>

void tick(int& tickcount, Vuart_controller* tb, VerilatedVcdC* tfp) {
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

void init(int& tickcount, Vuart_controller* tb, VerilatedVcdC* tfp) {
    tb->reset = 1;
    tick(tickcount, tb, tfp);
    tb->reset = 0;

    // needed to overwrite default of serial=0 (which indicates TX start)
    tb->serial_in = 1;
    tick(tickcount, tb, tfp);
}

int test_write_lock_acq(int& tickcount, Vuart_controller* tb, VerilatedVcdC* tfp) {
    tb->write_lock_req[0] = 1;
    tb->write_lock_req[1] = 1;
    tick(tickcount, tb, tfp);
    for (int i = 0; i < 3; i++) {
        tick(tickcount, tb, tfp);
        if (!tb->write_lock_res[0] || tb->write_lock_res[1]) {
            return SIM_ERROR;
        }
    }
    tb->write_lock_req[0] = 0;
    tb->write_lock_req[1] = 1;
    tick(tickcount, tb, tfp);
    if (tb->write_lock_res[0] || !tb->write_lock_res[1]) {
        return SIM_ERROR;
    }
    return SUCCESS;
}

int test_write_byte(int& tickcount, Vuart_controller* tb, VerilatedVcdC* tfp, char data, int index) {
    tb->data_in[index] = data;
    tb->data_in_valid[index] = 1;
    signal_err("uartctrl->write_ready", 1, tb->write_ready);
    tick(tickcount, tb, tfp);

    // wait until uart starts reading
    int i = 0;
    while (tb->write_ready) {
        i++;
        tick(tickcount, tb, tfp);
        if (i >= SYMBOL_TICK_COUNT) {
            return SIM_ERROR;
        }
    }
    tb->data_in_valid[index] = 0;

    // start bit
    for (int j = 1; j < SYMBOL_TICK_COUNT; j++) {
        tick(tickcount, tb, tfp);
        if (tb->serial_out) {
            return SIM_ERROR;
        }
    }
    
    // bits 0 - 7
    for (int i = 7; i >= 0; i--) {
        int last_bit = (data >> i) & 0x1;
        for (int j = 0; j < SYMBOL_TICK_COUNT; j++) {
            tick(tickcount, tb, tfp);
            if (last_bit != tb->serial_out) {
                return SIM_ERROR;
            }
            if (tb->write_ready) {
                return SIM_ERROR;
            }
        }
    }
    
    // end bit
    for (int j = 0; j < SYMBOL_TICK_COUNT; j++) {
        tick(tickcount, tb, tfp);
        if (tb->serial_out) {
            return SIM_ERROR;
        }
    }

    // end with write ready again
    if (tb->write_ready) {
        return SIM_ERROR;
    }
    return SUCCESS;

}

int test_read_bytes(int& tickcount, Vuart_controller* tb, VerilatedVcdC* tfp, int byte_count) {
    tb->data_in_valid[0] = 0;
    tb->data_in_valid[1] = 0;

    // write bytes to UART (as the "driver")
    for (unsigned int i = 0; i < byte_count; i++) {
        unsigned char data = static_cast<unsigned char>(i + 1);
        for (int j = 0; j < SYMBOL_TICK_COUNT; j++) {
            tick(tickcount, tb, tfp);
            tb->serial_in = 0;
        }
        for (int bit = 7; bit >= 0; bit--) {
            int last_bit = (data >> bit) & 0x1;
            for (int j = 0; j < SYMBOL_TICK_COUNT; j++) {
                tick(tickcount, tb, tfp);
                tb->serial_in = last_bit;
            }
        }
        for (int j = 0; j < SYMBOL_TICK_COUNT; j++) {
            tick(tickcount, tb, tfp);
            tb->serial_in = 0;
        }
    }

    // read bytes from UART (as the controller through FIFO)
    for (unsigned int i = 0; i < byte_count; i++) {
        tb->read_valid = 1;
        tick(tickcount, tb, tfp);
        if (!tb->data_out_valid) {
            printf("No data to read %d\n", i);
            return SIM_ERROR;
        }
        unsigned int data = static_cast<unsigned int>(tb->data_out);
        if (data != i + 1) {
            printf("Data incorrect %d %d\n", i + 1, data);
            return SIM_ERROR;
        }
    }
    return SUCCESS;
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Vuart_controller* tb = new Vuart_controller;
    Verilated::traceEverOn(true);
    
    int tickcount = 0;
    VerilatedVcdC* tfp = new VerilatedVcdC;
    tb->trace(tfp, 99);
    tfp->open("uart_ctrl.vcd");

    init(tickcount, tb, tfp);
    int res;

    res = test_write_lock_acq(tickcount, tb, tfp);
    if (res != SUCCESS) {
        printf("Test write lock acquire failed\n");
        tfp->close();
        return 0;
    }
    res = test_write_byte(tickcount, tb, tfp, 0x2A, 1);
    if (res != SUCCESS) {
        printf("Test write byte failed\n");
        tfp->close();
        return 0;
    }
    res = test_read_bytes(tickcount, tb, tfp, 5);
    if (res != SUCCESS) {
        printf("Test read 5 bytes failed\n");
        tfp->close();
        return 0;
    }

    printf("All tests passed\n");
    tfp->close();
    return 0;

}
