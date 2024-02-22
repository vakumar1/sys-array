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

#define BLOCKSIZE (MESHUNITS * MESHUNITS * TILEUNITS * TILEUNITS)

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
    unsigned char header;
    unsigned char bmem_addr;
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
    tb->imem_data = term_instr_to_bits(t);

    // verify thread returns to THREAD_DONE state
    tick(tickcount, tb, tfp);
    signal_err("tb->idle", 1, tb->idle);
    return;
}

void random_bmem_data(std::vector<int>& data) {
    data.clear();
    for (int i = 0; i < BLOCKSIZE; i++) {
        data.push_back(rand() % (1 << 16));
    }
    return;
}

void run_write_cmd(Vthread* tb, VerilatedVcdC* tfp, int& tickcount, unsigned int imem_addr, write_instr_t w) {
    // generate random data to request thread to write
    std::vector<int> data;
    random_bmem_data(data);

    // verify thread queries correct address
    unsigned int actual_imem_addr = tb->imem_addr;
    char err_msg[100];
    sprintf(err_msg, "Incorrect imem addr: expected=%d actual=%d", imem_addr, actual_imem_addr);
    condition_err(err_msg, [imem_addr, actual_imem_addr](){ return imem_addr != actual_imem_addr; });
    tb->imem_data = write_instr_to_bits(w);

    // verify thread goes to THREAD_READ_INST state
    tick(tickcount, tb, tfp);
    signal_err("tb->idle", 0, tb->idle);

    // verify thread goes to THREAD_WRITE_LOCK state and
    // i. requests write lock
    // ii. requests correct bmem addr
    // then grant lock req + provide bmem data + enable writes
    tick(tickcount, tb, tfp);
    signal_err("tb->idle", 0, tb->idle);
    signal_err("tb->write_lock_req", 1, tb->write_lock_req);
    unsigned int actual_bmem_addr = tb->bmem_addr;
    unsigned int expected_bmem_addr = w.bmem_addr << 8;
    sprintf(err_msg, "Incorrect bmem addr: expected=%d actual=%d", expected_bmem_addr, actual_imem_addr);
    condition_err(err_msg, [expected_bmem_addr, actual_bmem_addr](){ return expected_bmem_addr != actual_bmem_addr; });
    // data_err("tb->write_bmem_addr", w.bmem_addr << 8, tb->bmem_addr);
    tb->write_lock_res = 1;
    for (int i = 0; i < BLOCKSIZE; i++) {
        tb->bmem_data[i] = data[i];
    }
    tb->write_ready = 1;

    // verify thread goes to THREAD_WRITE_BYTECOUNT state and
    // writes correct byte count (4 bytes written)
    tick(tickcount, tb, tfp);
    signal_err("tb->idle", 0, tb->idle);
    signal_err("tb->write_data_valid", 0, tb->write_data_valid);
    unsigned int byte_count = 1 + 4 * BLOCKSIZE;
    unsigned char expected_byte;
    for (int i = 0; i < 4; i++) {
        tick(tickcount, tb, tfp);
        expected_byte = (unsigned char) ((byte_count >> (8 * i)) & 0xFF);
        signal_err("tb->idle", 0, tb->idle);
        signal_err("tb->write_data_valid", 1, tb->write_data_valid);
        data_err("tb->write_data", expected_byte, tb->write_data);
    }

    // verify thread goes to THREAD_WRITE_HEADER state and
    // writes correct header (1 byte written)
    tick(tickcount, tb, tfp);
    signal_err("tb->idle", 0, tb->idle);
    signal_err("tb->write_data_valid", 0, tb->write_data_valid);

    tick(tickcount, tb, tfp);
    signal_err("tb->idle", 0, tb->idle);
    signal_err("tb->write_data_valid", 1, tb->write_data_valid);
    data_err("tb->write_data", w.header, tb->write_data);

    // verify thread goes to THREAD_WRITE_DATA state and
    // writes correct data bytes (BLOCKSIZE bytes written)
    tick(tickcount, tb, tfp);
    signal_err("tb->idle", 0, tb->idle);
    signal_err("tb->write_data_valid", 0, tb->write_data_valid);
    for (int i = 0; i < BLOCKSIZE; i++) {
        unsigned int data_word = data[i];
        for (int j = 0; j < 4; j++) {
            tick(tickcount, tb, tfp);
            expected_byte = (unsigned char) ((data_word >> (8 * j)) & 0xFF);
            signal_err("tb->idle", 0, tb->idle);
            signal_err("tb->write_data_valid", 1, tb->write_data_valid);
            data_err("tb->write_data", expected_byte, tb->write_data);
        }
    }

    // verify thread requests lock release and oblige
    tick(tickcount, tb, tfp);
    signal_err("tb->write_lock_req", 0, tb->write_lock_req);
    tb->write_lock_res = 0;

    // verify thread goes to THREAD_READ_INST state
    // with pc += 4
    // * this is different from actual expected behavior since here 
    //   setting tb->write_lock_res occurs synchronously (BEFORE this tick)
    //   while the uart controller will set write_lock_res (immediately AFTER this tick)
    tick(tickcount, tb, tfp);
    signal_err("tb->idle", 0, tb->idle);
    signal_err("tb->write_data_valid", 0, tb->write_data_valid);
    actual_imem_addr = tb->imem_addr;
    sprintf(err_msg, "Incorrect next imem addr: expected=%d actual=%d", imem_addr + 4, actual_imem_addr);
    condition_err(err_msg, [&imem_addr, &actual_imem_addr](){ return imem_addr + 4 != actual_imem_addr; });
}

void run_cmds(Vthread* tb, VerilatedVcdC* tfp, int& tickcount,
                std::vector<instr_t>& instructions) {
    tb->running = 1;
    tick(tickcount, tb, tfp);
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
    test_runner(tfp, "[THREAD]", "WRITE(S) + TERM", 
        [&tb, &tfp, &tickcount](){
            instr_t term_inst;
            term_inst.type = TERM;
            
            instr_t write_inst;
            write_inst.type = WRITE;

            std::vector<instr_t> instructions;
            for (int i = 0; i < 42; i++) {
                write_inst.inner_instr.w = { (unsigned char) rand(), (unsigned char) rand() };
                instructions.push_back(write_inst);
            }
            term_inst.inner_instr.t = {};
            instructions.push_back(term_inst);

            run_cmds(tb, tfp, tickcount, instructions);
        });


    printf("All tests passed\n");
    return 0;
}
