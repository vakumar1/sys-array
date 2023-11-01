#include "utils.h"

#include "Vuart.h"
#include "verilated.h"
#include "verilated_vcd_c.h"

#define SYMBOL_TICK_COUNT 1085

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

int send_byte(Vuart* sender, Vuart* receiver, VerilatedVcdC* sender_tfp, VerilatedVcdC* receiver_tfp, char data) {
    int sender_tickcount = 0;
    int receiver_tickcount = 0;

    // init
    sender->reset = 1;
    receiver->reset = 1;
    tick(sender_tickcount, sender, sender_tfp);
    tick(receiver_tickcount, receiver, receiver_tfp);
    sender->reset = 0;
    receiver->reset = 0;
    sender_tick(sender, sender_tickcount, sender_tfp, data, 1, receiver->rts);
    receiver_tick(receiver, receiver_tickcount, receiver_tfp, sender->serial_out);

    // initialize transmitter to START state 
    for (int i = 1; i < SYMBOL_TICK_COUNT; i++) {
        if (!receiver->rts) {
            return SIM_ERROR;
        }
        sender_tick(sender, sender_tickcount, sender_tfp, data, 1, receiver->rts);
        receiver_tick(receiver, receiver_tickcount, receiver_tfp, sender->serial_out);
    }    

    // send initial bit and initialize receiver to READING state
    for (int i = 0; i < SYMBOL_TICK_COUNT; i++) {
        if (sender->serial_out || receiver->rts) {
            return SIM_ERROR;
        }
        sender_tick(sender, sender_tickcount, sender_tfp, data, 1, receiver->rts);
        receiver_tick(receiver, receiver_tickcount, receiver_tfp, sender->serial_out);
    }

    // send bits 0-7
    for (int bit_pos = 7; bit_pos >= 0; bit_pos--) {
        char bit = (data >> bit_pos) & 0x1;
        for (int i = 0; i < SYMBOL_TICK_COUNT; i++) {
            if (sender->serial_out != bit || receiver->rts) {
                return SIM_ERROR;
            }
            sender_tick(sender, sender_tickcount, sender_tfp, data, 1, receiver->rts);
            receiver_tick(receiver, receiver_tickcount, receiver_tfp, sender->serial_out);
            
        }
    }

    // send final bit to close connection
    char result = 0x0;
    bool found = false;
    for (int i = 0; i < SYMBOL_TICK_COUNT; i++) {
        if ((i == 0 && !receiver->data_out_valid) || (i != 0 && receiver->data_out_valid)) {
            return SIM_ERROR;
        }
        result = receiver->data_out;
        found = true;
        sender_tick(sender, sender_tickcount, sender_tfp, data, 1, receiver->rts);
        receiver_tick(receiver, receiver_tickcount, receiver_tfp, sender->serial_out);
    }

    if (result != data) {
        return SIM_ERROR;
    }
    return SUCCESS;

}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Vuart* sender = new Vuart;
    Vuart* receiver = new Vuart;
    Verilated::traceEverOn(true);
    
    VerilatedVcdC* sender_tfp = new VerilatedVcdC;
    sender->trace(sender_tfp, 99);
    sender_tfp->open("uart_sender.vcd");
    VerilatedVcdC* receiver_tfp = new VerilatedVcdC;
    receiver->trace(receiver_tfp, 99);
    receiver_tfp->open("uart_receiver.vcd");

    int result = send_byte(sender, receiver, sender_tfp, receiver_tfp, 0x2A);
    printf("Send byte UART->UART %s\n", result == SUCCESS ? "passed" : "failed");
    return 0;
}
