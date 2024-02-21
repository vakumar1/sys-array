#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include "Vthread.h"
#include "verilated.h"
#include "verilated_vcd_c.h"
#include <vector>
#include <string>

// DEFAULT MESH PARAMETERS: 256 mesh
#ifndef BITWIDTH
#define BITWIDTH 32
#endif

#ifndef MESHUNITS
#define MESHUNITS 4
#endif

#ifndef TILEUNITS
#define TILEUNITS 4
#endif

//
// INSTRUCTION DATA TYPES
//

#define TERM_CODE 0x0
#define WRITE_CODE 0x1

enum instr_type {
    TERM,
    WRITE
} ;

// TERM instr.
typedef struct {} 
term_instr_t;

unsigned int term_instr_to_bits(term_instr_t t) {
    return TERM_CODE;
}

// WRITE instr.
typedef struct {
    char header;
    char bmem_addr;
} write_instr_t;

unsigned int write_instr_to_bits(write_instr_t w) {
    return 0 | (w.header << 10) | (w.bmem_addr << 2) | (WRITE_CODE);
} 

// instr. wrapper
typedef struct {
    instr_type type;
    union {
        term_instr_t t;
        write_instr_t w;
    } inner_instr;
} instr_t;


void tick(int& tickcount, Vthread* tb, VerilatedVcdC* tfp) {
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

void init(int& tickcount, Vthread* tb, VerilatedVcdC* tfp) {
    tb->reset = 1;
    tick(tickcount, tb, tfp);
    tb->reset = 0;
    tick(tickcount, tb, tfp);
}

//
// COMMAND RUNNERS
//

void run_term_cmd(Vthread* tb, VerilatedVcdC* tfp, int& tickcount, unsigned int imem_addr, term_instr_t t) {
    // verify thread queries correct address -> pass addr
    unsigned int actual_imem_addr = tb->imem_addr;
    char err_msg[100];
    sprintf(err_msg, "Incorrect imem addr: expected=%d actual=%d", imem_addr, actual_imem_addr);
    condition_err(err_msg, [&imem_addr, &actual_imem_addr](){ return imem_addr != actual_imem_addr; });
    tb->imem_data = imem_addr;

    // verify thread returns to THREAD_DONE state
    tick(tickcount, tb, tfp);
    data_err("tb->idle", 1, tb->idle);
    return;
}

void run_write_cmd(Vthread* tb, VerilatedVcdC* tfp, int& tickcount, unsigned int imem_addr, write_instr_t w) {
    // verify thread queries correct address
    unsigned int actual_imem_addr = tb->imem_addr;
    char err_msg[100];
    sprintf(err_msg, "Incorrect imem addr: expected=%d actual=%d", imem_addr, actual_imem_addr);
    condition_err(err_msg, [imem_addr, actual_imem_addr](){ return imem_addr != actual_imem_addr; });

    // TODO: add write cmd logic verification
}

void run_cmds(Vthread* tb, VerilatedVcdC* tfp, int& tickcount,
                std::vector<instr_t>& instructions) {
    for (unsigned int i = 0; i < instructions.size(); i++) {
        unsigned int imem_addr = i * 4;
        instr_t inst = instructions[i];
        switch (inst.type) {
            case WRITE:
                run_write_cmd(tb, tfp, tickcount, imem_addr, inst.inner_instr.w);
                break;
            case TERM:
                run_term_cmd(tb, tfp, tickcount, imem_addr, inst.inner_instr.t);
                break;
            default:
                break;
        }
    }
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Vthread* tb = new Vthread;
    Verilated::traceEverOn(true);
    
    int tickcount = 0;
    VerilatedVcdC* tfp = new VerilatedVcdC;
    tb->trace(tfp, 99);
    tfp->open("thread.vcd");

    init(tickcount, tb, tfp);

    instr_t term_inst;
    term_inst.type = TERM;
    term_inst.inner_instr.t = {};

    std::vector<instr_t> instructions;
    instructions.push_back(term_inst);
    run_cmds(tb, tfp, tickcount, instructions);
    printf("All tests passed\n");
    return 0;
}
