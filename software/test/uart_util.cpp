#include "uart_util.h"

#include <vector>

#define SYMBOL_TICK_COUNT 1085

//
// RECEIVER UTILS
//

void receiver_init(int& tickcount, Vuart* receiver, VerilatedVcdC* tfp) {
    receiver->reset = 1;
    tick(tickcount, receiver, tfp);
    receiver->reset = 0;
    receiver_tick(receiver, tickcount, tfp, 1);
}

void receiver_tick(Vuart* receiver, int& tickcount, VerilatedVcdC* tfp,
                    char sender_serial_out) {
    receiver->reset = 0;
    receiver->data_in = 0;                      // unused
    receiver->data_in_valid = 0;                // unused
    receiver->serial_in = sender_serial_out;
    receiver->local_ready = 1;
    receiver->cts = 0;                          // unused
    tick(tickcount, receiver, tfp);
}


//
// SENDER UTILS
//

// returns an array of bits corresponding to the serial line sent by the transmitter sending a byte
void sender_byte_bits(char data, std::vector<int>& result) {
    result.resize(0);

    // send initial bit and initialize receiver to READING state
    for (int i = 0; i < SYMBOL_TICK_COUNT; i++) {
        result.push_back(0);
    }

    // send bits 0-7
    for (int bit_pos = 7; bit_pos >= 0; bit_pos--) {
        char bit = (data >> bit_pos) & 0x1;
        for (int i = 0; i < SYMBOL_TICK_COUNT; i++) {
            result.push_back(bit);
        }
    }

    // send final bit to close connection
    for (int i = 0; i < SYMBOL_TICK_COUNT; i++) {
        result.push_back(0);
    }

    return;
}

void sender_init(int& tickcount, Vuart* sender, VerilatedVcdC* tfp) {
    sender->reset = 1;
    tick(tickcount, sender, tfp);
    sender->reset = 0;
    sender_tick(sender, tickcount, tfp, 0x0, 0, 1);
}

void sender_tick(Vuart* sender, int& tickcount, VerilatedVcdC* tfp,
                    char data, char data_valid, char receiver_rts) {
    sender->reset = 0;
    sender->data_in = data;
    sender->data_in_valid = data_valid;
    sender->serial_in = 1;                      // unused
    sender->local_ready = 0;                    // unused
    sender->cts = receiver_rts;
    tick(tickcount, sender, tfp);
}

//
// GENERAL
//

// tick for a uart module
void tick(int& tickcount, Vuart* tb, VerilatedVcdC* tfp) {
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
