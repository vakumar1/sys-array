#include "utils.h"
#include "uart_util.h"

#include "Vuart.h"
#include "verilated.h"
#include "verilated_vcd_c.h"

#include <stdexcept>
#include <string>

int send_byte(int& sender_tickcount, Vuart* sender, int& receiver_tickcount, Vuart* receiver, 
                VerilatedVcdC* sender_tfp, VerilatedVcdC* receiver_tfp, char data) {

    // check that rx is ready to receive
    sender_tick(sender, sender_tickcount, sender_tfp, 0x0, 0, receiver->rts);
    receiver_tick(receiver, receiver_tickcount, receiver_tfp, sender->serial_out);
    uart_signal_err(0, 0, "receiver->rts", receiver->rts, 1);

    // SUMMARY OF EXECUTION:
    // each for loop sends a single bit over the UART line
    // the rule is that we enter into iteration i AFTER the rising edge
    // where the tick counter is set to i
    //
    //              ___________             ___________
    //  ___________|           |___________|
    //             iter=i^ (in for loop)
    //   tick_ctr=i^ (in code)

    // initialize sender with data
    // sets tick_ctr=0 for sender and receiver
    sender_tick(sender, sender_tickcount, sender_tfp, data, 1, receiver->rts);
    receiver_tick(receiver, receiver_tickcount, receiver_tfp, sender->serial_out);


    // send initial bit and initialize receiver to READING state
    for (int i = 0; i < SYMBOL_TICK_COUNT; i++) {
        // after setting tick_ctr=i for i in 0..SYMBOL_TICK_COUNT-1
        // check sender signal matches bit value
        // (initial bit = 0 for UART)
        uart_signal_err(0, i, "sender->serial_out", sender->serial_out, 0);
        uart_signal_err(0, i, "receiver->rts", receiver->rts, 0);

        // update the signal: send tick_ctr from i -> i + 1
        sender_tick(sender, sender_tickcount, sender_tfp, 0x0, 0, receiver->rts);
        receiver_tick(receiver, receiver_tickcount, receiver_tfp, sender->serial_out);
    }

    // send bits 0-7
    for (int bit_pos = 7; bit_pos >= 0; bit_pos--) {
        char bit = (data >> bit_pos) & 0x1;
        for (int i = 0; i < SYMBOL_TICK_COUNT; i++) {
            // check sender signal matches bit value
            // UART is little endian -> bit i = ith bit from the left
            uart_signal_err(bit_pos + 1, i, "sender->serial_out", sender->serial_out, bit);
            uart_signal_err(bit_pos + 1, i, "receiver->rts", receiver->rts, 0);

            // update the signal: send tick_ctr from i -> i + 1
            sender_tick(sender, sender_tickcount, sender_tfp, data, 1, receiver->rts);
            receiver_tick(receiver, receiver_tickcount, receiver_tfp, sender->serial_out);
            
        }
    }

    // send final bit to close connection
    char result = 0x0;
    for (int i = 0; i < SYMBOL_TICK_COUNT; i++) {
        // the receiver should output the data for only the FIRST clock cycle
        // after it is on the finish bit
        // (all following output data should be invalid)
        if (i == 0) {
            uart_signal_err(9, i, "receiver->data_out_valid", receiver->data_out_valid, 1);
        } else {
            uart_signal_err(9, i, "receiver->data_out_valid", receiver->data_out_valid, 0);
        }
        result = receiver->data_out;

        // update the signal: send tick_ctr from i -> i + 1
        sender_tick(sender, sender_tickcount, sender_tfp, data, 1, receiver->rts);
        receiver_tick(receiver, receiver_tickcount, receiver_tfp, sender->serial_out);
    }

    // verify
    data_err("UART[0]", result, data);
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

    int sender_tickcount = 0;
    int receiver_tickcount = 0;
    sender_init(sender_tickcount, sender, sender_tfp);
    receiver_init(receiver_tickcount, receiver, receiver_tfp);
    
    int result = send_byte(sender_tickcount, sender, receiver_tickcount, receiver, sender_tfp, receiver_tfp, 0x2A);
    printf("Send byte UART->UART %s\n", result == SUCCESS ? "passed" : "failed");
    return 0;
}
