#include "utils/test_utils.h"
#include "utils/instr_utils.h"

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
    condition_err(err_msg, imem_addr != actual_imem_addr);
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
    condition_err(err_msg, imem_addr != actual_imem_addr);
    tb->imem_data = write_instr_to_bits(w);

    // verify thread starts in THREAD_READ_INST state
    tick(tickcount, tb, tfp);
    signal_err("tb->idle", 0, tb->idle);

    // verify thread goes to THREAD_WRITE_ACQ_LOCK state and
    // i. requests write lock
    // ii. requests correct bmem addr
    // then grant lock req + provide bmem data + enable writes
    tick(tickcount, tb, tfp);
    signal_err("tb->idle", 0, tb->idle);
    signal_err("tb->write_lock_req", 1, tb->write_lock_req);
    unsigned int actual_bmem_addr = tb->bmem_addr;
    unsigned int expected_bmem_addr = w.bmem_addr << 8;
    sprintf(err_msg, "Incorrect bmem addr: expected=%d actual=%d", expected_bmem_addr, actual_imem_addr);
    condition_err(err_msg, expected_bmem_addr != actual_bmem_addr);
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

    // verify thread goes to THREAD_WRITE_REL_LOCK state and
    // requests lock release and oblige
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
    condition_err(err_msg, imem_addr + 4 != actual_imem_addr);
}

void run_load_cmd(Vthread* tb, VerilatedVcdC* tfp, int& tickcount, unsigned int imem_addr, load_instr_t l) {
    // verify thread queries correct address
    unsigned int actual_imem_addr = tb->imem_addr;
    char err_msg[100];
    sprintf(err_msg, "Incorrect imem addr: expected=%d actual=%d", imem_addr, actual_imem_addr);
    condition_err(err_msg, imem_addr != actual_imem_addr);
    tb->imem_data = load_instr_to_bits(l);

    // verify thread starts in THREAD_READ_INST state
    tick(tickcount, tb, tfp);
    signal_err("tb->idle", 0, tb->idle);

    // verify thread goes to THREAD_LOAD_ACQ_LOCK state and
    // requests load lock
    // wait a random number of cycles before granting lock
    for (int i = 0; i < 1 + rand() % 5; i++) {
        tick(tickcount, tb, tfp);
        signal_err("tb->idle", 0, tb->idle);
        signal_err("tb->load_lock_req", 1, tb->load_lock_req);
        tb->load_lock_res = 0;
    }
    tb->load_lock_res = 1;

    // verify thread goes to THREAD_LOAD_WAIT state and
    // waits until load finishes
    // wait a random number of cycles before completing
    for (int i = 0; i < 1 + rand() % 5; i++) {
        tick(tickcount, tb, tfp);
        signal_err("tb->idle", 0, tb->idle);
        signal_err("tb->load_lock_req", 1, tb->load_lock_req);
        tb->load_finished = 0;
    }
    tb->load_finished = 1;

    // verify thread goes to THREAD_LOAD_REL_LOCK state and
    // requests lock release
    tick(tickcount, tb, tfp);
    signal_err("tb->load_lock_req", 0, tb->load_lock_req);
    tb->load_lock_res = 0;

    // verify thread goes to THREAD_READ_INST state
    // with pc += 4
    tick(tickcount, tb, tfp);
    signal_err("tb->idle", 0, tb->idle);
    actual_imem_addr = tb->imem_addr;
    sprintf(err_msg, "Incorrect next imem addr: expected=%d actual=%d", imem_addr + 4, actual_imem_addr);
    condition_err(err_msg, imem_addr + 4 != actual_imem_addr);
}

void run_comp_cmd(Vthread* tb, VerilatedVcdC* tfp, int& tickcount, unsigned int imem_addr, comp_instr_t c) {
    // verify thread queries correct address
    unsigned int actual_imem_addr = tb->imem_addr;
    char err_msg[100];
    sprintf(err_msg, "Incorrect imem addr: expected=%d actual=%d", imem_addr, actual_imem_addr);
    condition_err(err_msg, imem_addr != actual_imem_addr);
    tb->imem_data = comp_instr_to_bits(c);

    // verify thread starts in THREAD_READ_INST state
    tick(tickcount, tb, tfp);
    signal_err("tb->idle", 0, tb->idle);

    // verify thread goes to THREAD_COMP_ACQ_LOCK state and
    // requests comp lock
    // wait a random number of cycles before granting lock
    for (int i = 0; i < 1 + rand() % 5; i++) {
        tick(tickcount, tb, tfp);
        signal_err("tb->idle", 0, tb->idle);
        signal_err("tb->comp_lock_req", 1, tb->comp_lock_req);
        tb->comp_lock_res = 0;
    }
    tb->comp_lock_res = 1;

    // verify thread goes to THREAD_COMP_WAIT state and
    // waits until comp finishes
    // wait a random number of cycles before completing
    for (int i = 0; i < 1 + rand() % 5; i++) {
        tick(tickcount, tb, tfp);
        signal_err("tb->idle", 0, tb->idle);
        signal_err("tb->comp_lock_req", 1, tb->comp_lock_req);
        tb->comp_finished = 0;
    }
    tb->comp_finished = 1;

    // verify thread goes to THREAD_COMP_REL_LOCK state and
    // requests lock release
    tick(tickcount, tb, tfp);
    signal_err("tb->comp_lock_req", 0, tb->comp_lock_req);
    tb->comp_lock_res = 0;

    // verify thread goes to THREAD_READ_INST state
    // with pc += 4
    tick(tickcount, tb, tfp);
    signal_err("tb->idle", 0, tb->idle);
    actual_imem_addr = tb->imem_addr;
    sprintf(err_msg, "Incorrect next imem addr: expected=%d actual=%d", imem_addr + 4, actual_imem_addr);
    condition_err(err_msg, imem_addr + 4 != actual_imem_addr);
}

void run_cmds(Vthread* tb, VerilatedVcdC* tfp, int& tickcount,
                std::vector<instr_t>& instructions) {
    // enter THREAD_IDLE state
    tb->enabled = 1;
    tick(tickcount, tb, tfp);
    signal_err("tb->idle", 1, tb->idle);

    // enter THREAD_READ_INST state
    tb->start = 1;
    tick(tickcount, tb, tfp);
    signal_err("tb->idle", 0, tb->idle);
    tb->start = 0;

    for (unsigned int i = 0; i < instructions.size(); i++) {
        // force thread into THREAD_READ_INST state after TERM inst.
        if (i > 0 && instructions[i - 1].type == TERM) {
            tb->start = 1;
            tick(tickcount, tb, tfp);
            tb->start = 0;
        }

        unsigned int imem_addr = i * 4;
        instr_t inst = instructions[i];
        switch (inst.type) {
            case WRITE:
                run_write_cmd(tb, tfp, tickcount, imem_addr, inst.inner_instr.w);
                break;
            case TERM:
                run_term_cmd(tb, tfp, tickcount, imem_addr, inst.inner_instr.t);
                break;
            case LOAD:
                run_load_cmd(tb, tfp, tickcount, imem_addr, inst.inner_instr.l);
                break;
            case COMP:
                run_comp_cmd(tb, tfp, tickcount, imem_addr, inst.inner_instr.c);
                break;
            default:
                break;
        }
    }
    tb->enabled = 0;
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
    test_runner("[THREAD]", "TERM", 
        [&tb, &tfp, &tickcount](){
            instr_t term_inst;
            term_inst.type = TERM;

            std::vector<instr_t> instructions;
            term_inst.inner_instr.t = {};
            instructions.push_back(term_inst);

            run_cmds(tb, tfp, tickcount, instructions);
        },
        [&tfp](){
            tfp->close();
        });

    init(tickcount, tb, tfp);
    test_runner("[THREAD]", "WRITES + TERM", 
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
        },
        [&tfp](){
            tfp->close();
        });

    init(tickcount, tb, tfp);
    test_runner("[THREAD]", "WRITES/LOADS/COMPS + TERM", 
        [&tb, &tfp, &tickcount](){
            instr_t term_inst;
            term_inst.type = TERM;
            
            instr_t write_inst;
            write_inst.type = WRITE;

            instr_t load_inst;
            load_inst.type = LOAD;

            instr_t comp_inst;
            comp_inst.type = COMP;

            std::vector<instr_t> instructions;
            int inst_code;
            for (int i = 0; i < 42; i++) {
                inst_code = rand() % 3;
                if (inst_code == 0) {
                    write_inst.inner_instr.w = { (unsigned char) rand(), (unsigned char) rand() };
                    instructions.push_back(write_inst);
                } else if (inst_code == 1) {
                    load_inst.inner_instr.l = { (unsigned char) rand() };
                    instructions.push_back(load_inst);
                } else if (inst_code == 2) {
                    comp_inst.inner_instr.c = { (unsigned char) rand(), (unsigned char) rand(), (unsigned char) rand() };
                    instructions.push_back(comp_inst);
                }
            }
            term_inst.inner_instr.t = {};
            instructions.push_back(term_inst);

            run_cmds(tb, tfp, tickcount, instructions);
        },
        [&tfp](){
            tfp->close();
        });

    tfp->close();
    printf("All tests passed\n");
    return 0;
}
