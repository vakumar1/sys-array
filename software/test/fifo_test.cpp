#include "utils.h"

#include "Vfifo.h"
#include "verilated.h"
#include "verilated_vcd_c.h"

void tick(int& tickcount, Vfifo* tb, VerilatedVcdC* tfp) {
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

int fill_and_empty_test(Vfifo* fifo, VerilatedVcdC* tfp) {
    int tickcount = 0;
    
    // init
    fifo->reset = 1;
    tick(tickcount, fifo, tfp);
    fifo->reset = 0;
    fifo->write = 0;
    fifo->read = 0;
    tick(tickcount, fifo, tfp);

    // check fifo is empty
    if (!fifo->empty) {
        return SIM_ERROR;
    }

    // add data to fifo until full
    for (int i = 0; i < 256; i++) {
        char data = static_cast<char>(i);
        if (fifo->data_out_valid || fifo->full) {
            return SIM_ERROR;
        }
        fifo->data_in = data;
        fifo->write = 1;
        fifo->read = 0;
        tick(tickcount, fifo, tfp);
    }
    if (!fifo->full) {
        return SIM_ERROR;
    }

    // try writing (this should fail)
    fifo->data_in = 0x2A;
    fifo->write = 1;
    fifo->read = 0;
    tick(tickcount, fifo, tfp);

    // remove data until empty
    for (int i = 0; i < 256; i++) {
        char expected_data = static_cast<char>(i);
        if (fifo->empty) {
            return SIM_ERROR;
        }
        fifo->write = 0;
        fifo->read = 1;
        tick(tickcount, fifo, tfp);
        char res = fifo->data_out;
        if (!fifo->data_out_valid || res != expected_data) {
            return SIM_ERROR;
        }
    }
    if (!fifo->empty) {
        return SIM_ERROR;
    }

    // try reading (this should fail)
    fifo->write = 1;
    fifo->read = 0;
    tick(tickcount, fifo, tfp);
    if (fifo->data_out_valid) {
        return SIM_ERROR;
    }

    return SUCCESS;
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Vfifo* fifo = new Vfifo;
    Verilated::traceEverOn(true);
    
    VerilatedVcdC* tfp = new VerilatedVcdC;
    fifo->trace(tfp, 99);
    tfp->open("fifo.vcd");

    int result = fill_and_empty_test(fifo, tfp);
    printf("Fifo empty->full->empty %s\n", result == SUCCESS ? "passed" : "failed");
    return 0;
}
