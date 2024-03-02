#include "utils/test_utils.h"
#include "utils/uart_utils.h"

#include "Vuart_controller.h"
#include "verilated.h"
#include "verilated_vcd_c.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdexcept>
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
        signal_err("tb->write_lock_res[0]", 1, tb->write_lock_res[0]);
        signal_err("tb->write_lock_res[1]", 0, tb->write_lock_res[1]);
    }
    tb->write_lock_req[0] = 0;
    tb->write_lock_req[1] = 1;
    tick(tickcount, tb, tfp);
    signal_err("tb->write_lock_res[0]", 0, tb->write_lock_res[0]);
    signal_err("tb->write_lock_res[1]", 1, tb->write_lock_res[1]);
    return SUCCESS;
}

int test_write_byte(int& tickcount, Vuart_controller* tb, VerilatedVcdC* tfp, char data, int index) {
    tb->data_in[index] = data;
    tb->data_in_valid[index] = 1;
    signal_err("uartctrl->write_ready", 1, tb->write_ready);
    tick(tickcount, tb, tfp);

    // wait until uart starts writing out
    for (int i = 0; ; i++) {
        if (tb->serial_out == 0) {
            break;
        }
        tick(tickcount, tb, tfp);
        condition_err("UART read ready timeout", i >= SYMBOL_TICK_COUNT);
    }
    tb->data_in_valid[index] = 0;

    // start bit
    for (int j = 1; j < SYMBOL_TICK_COUNT; j++) {
        tick(tickcount, tb, tfp);
        signal_err("tb->serial_out", 0, tb->serial_out);
    }
    
    // bits 0 - 7
    for (int i = 7; i >= 0; i--) {
        int last_bit = (data >> i) & 0x1;
        for (int j = 0; j < SYMBOL_TICK_COUNT; j++) {
            tick(tickcount, tb, tfp);
            signal_err("tb->serial_out", last_bit, tb->serial_out);
        }
    }
    
    // end bit
    for (int j = 0; j < SYMBOL_TICK_COUNT; j++) {
        tick(tickcount, tb, tfp);
        signal_err("tb->serial_out", 0, tb->serial_out);
    }

    // end with write ready again
    tick(tickcount, tb, tfp);
    signal_err("tb->serial_out", 1, tb->serial_out);
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
        signal_err("tb->data_out_valid", 1, tb->data_out_valid);

        unsigned int data = static_cast<unsigned int>(tb->data_out);
        data_err("tb->data_out", i + 1, data);
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

    test_runner("[UART CTRL]", "Write lock acquire",
        [&tickcount, &tb, &tfp](){ 
            test_write_lock_acq(tickcount, tb, tfp);
        },
        [&tfp](){
            tfp->close();
        });
    test_runner("[UART CTRL]", "UART write",
        [&tickcount, &tb, &tfp](){ 
            test_write_byte(tickcount, tb, tfp, 0x2A, 1);
        },
        [&tfp](){
            tfp->close();
        });
    test_runner("[UART CTRL]", "UART read",
        [&tickcount, &tb, &tfp](){ 
            test_read_bytes(tickcount, tb, tfp, 5);
        },
        [&tfp](){
            tfp->close();
        });
    printf("All tests passed\n");
    tfp->close();
    return 0;
}
