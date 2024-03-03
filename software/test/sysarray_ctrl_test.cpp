#include "utils/test_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include "Vsys_array_controller.h"
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

// INPUT MATRIX ENTRY MAX
#define MAX_INP (1 << ((BITWIDTH / 2) - 2)) / (MESHUNITS * TILEUNITS)

#define MATSIZE MESHUNITS * MESHUNITS * TILEUNITS * TILEUNITS

// DUMMY ADDRESSES FOR B0, B1, A, D, C
// ("MOCK MEMORY")
unsigned int b0_addr = 0 * MATSIZE;
unsigned int b1_addr = 1 * MATSIZE;
unsigned int a_addr = 2 * MATSIZE;
unsigned int d_addr = 3 * MATSIZE;
unsigned int c_addr = 4 * MATSIZE;

void tick(int& tickcount, Vsys_array_controller* tb, VerilatedVcdC* tfp) {
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

void init(int& tickcount, Vsys_array_controller* tb, VerilatedVcdC* tfp) {
    tb->reset = 1;
    tick(tickcount, tb, tfp);
    tb->reset = 0;
    tick(tickcount, tb, tfp);
}

// REQUESTS

// test that competing load requests
// resolves to the lowest index
int double_load_req_conflict(int& tickcount, Vsys_array_controller* tb, VerilatedVcdC* tfp) {
    tb->load_lock_req[0] = 1;
    tb->load_lock_req[1] = 1;
    tb->comp_lock_req[0] = 0;
    tb->comp_lock_req[1] = 0;
    tb->B_addr[0] = b0_addr;
    tb->B_addr[1] = b1_addr;
    tick(tickcount, tb, tfp);
    tb->load_lock_req[0] = 0;
    tb->load_lock_req[1] = 0;

    signal_err("tb->load_lock_res[0]", 1, tb->load_lock_res[0]);
    signal_err("tb->load_lock_res[1]", 0, tb->load_lock_res[1]);
    signal_err("tb->comp_lock_res[0]", 0, tb->comp_lock_res[0]);
    signal_err("tb->comp_lock_res[1]", 0, tb->comp_lock_res[1]);
    return SUCCESS;
}

// test that non-competing load request is granted
int single_load_req(int& tickcount, Vsys_array_controller* tb, VerilatedVcdC* tfp, int index) {
    int load_idx = index;
    int idle_idx = 1 - index;
    int load_addr = index == 0 ? b0_addr : b1_addr;
    int idle_addr = index == 0 ? b1_addr : b0_addr;
    tb->load_lock_req[load_idx] = 1;
    tb->load_lock_req[idle_idx] = 0;
    tb->comp_lock_req[load_idx] = 0;
    tb->comp_lock_req[idle_idx] = 0;
    tb->B_addr[load_idx] = load_addr;
    tb->B_addr[idle_idx] = idle_addr;
    tick(tickcount, tb, tfp);
    tb->load_lock_req[load_idx] = 0;
    tb->load_lock_req[idle_idx] = 0;

    signal_err("tb->load_lock_res[load_idx]", 1, tb->load_lock_res[load_idx]);
    signal_err("tb->load_lock_res[idle_idx]", 0, tb->load_lock_res[idle_idx]);
    signal_err("tb->comp_lock_res[load_idx]", 0, tb->comp_lock_res[load_idx]);
    signal_err("tb->comp_lock_res[idle_idx]", 0, tb->comp_lock_res[idle_idx]);
    return SUCCESS;
}

// test that non-competing comp request is granted
int single_comp_req(int& tickcount, Vsys_array_controller* tb, VerilatedVcdC* tfp, int index) {
    int comp_idx = index;
    int idle_idx = 1 - index;
    tb->load_lock_req[comp_idx] = 0;
    tb->load_lock_req[idle_idx] = 0;
    tb->comp_lock_req[comp_idx] = 1;
    tb->comp_lock_req[idle_idx] = 0;
    tb->A_addr[comp_idx] = a_addr;
    tb->D_addr[comp_idx] = d_addr;
    tb->C_addr[comp_idx] = c_addr;
    tick(tickcount, tb, tfp);
    tb->comp_lock_req[comp_idx] = 0;
    tb->comp_lock_req[idle_idx] = 0;

    signal_err("tb->load_lock_res[comp_idx]", 0, tb->load_lock_res[comp_idx]);
    signal_err("tb->load_lock_res[idle_idx]", 0, tb->load_lock_res[idle_idx]);
    signal_err("tb->comp_lock_res[comp_idx]", 1, tb->comp_lock_res[comp_idx]);
    signal_err("tb->comp_lock_res[idle_idx]", 0, tb->comp_lock_res[idle_idx]);
    return SUCCESS;
}

// test that competing comp requests
// resolves to the lowest index
int double_comp_req_conflict(int& tickcount, Vsys_array_controller* tb, VerilatedVcdC* tfp) {
    tb->load_lock_req[0] = 0;
    tb->load_lock_req[1] = 0;
    tb->comp_lock_req[0] = 1;
    tb->comp_lock_req[1] = 1;
    tb->A_addr[0] = a_addr;
    tb->D_addr[0] = d_addr;
    tb->C_addr[0] = c_addr;
    tick(tickcount, tb, tfp);
    tb->comp_lock_req[0] = 0;
    tb->comp_lock_req[1] = 0;

    signal_err("tb->load_lock_res[0]", 0, tb->load_lock_res[0]);
    signal_err("tb->load_lock_res[1]", 0, tb->load_lock_res[1]);
    signal_err("tb->comp_lock_res[0]", 1, tb->comp_lock_res[0]);
    signal_err("tb->comp_lock_res[1]", 0, tb->comp_lock_res[1]);
    return SUCCESS;
}

// test that non-competing load and comp requests
// are granted to each thread
int load_and_comp_req_no_conflict(int& tickcount, Vsys_array_controller* tb, VerilatedVcdC* tfp, int load_idx) {
    int comp_idx = 1 - load_idx;
    int b_addr = load_idx == 0 ? b0_addr : b1_addr;
    tb->load_lock_req[load_idx] = 1;
    tb->load_lock_req[comp_idx] = 0;
    tb->comp_lock_req[load_idx] = 0;
    tb->comp_lock_req[comp_idx] = 1;
    tb->B_addr[load_idx] = b_addr;
    tb->A_addr[comp_idx] = a_addr;
    tb->D_addr[comp_idx] = d_addr;
    tb->C_addr[comp_idx] = c_addr;
    tick(tickcount, tb, tfp);
    tb->load_lock_req[load_idx] = 0;
    tb->load_lock_req[comp_idx] = 0;
    tb->comp_lock_req[load_idx] = 0;
    tb->comp_lock_req[comp_idx] = 0;

    signal_err("tb->load_lock_res[load_idx]", 1, tb->load_lock_res[load_idx]);
    signal_err("tb->load_lock_res[comp_idx]", 0, tb->load_lock_res[comp_idx]);
    signal_err("tb->comp_lock_res[load_idx]", 0, tb->comp_lock_res[load_idx]);
    signal_err("tb->comp_lock_res[comp_idx]", 1, tb->comp_lock_res[comp_idx]);
    return SUCCESS;
}


// EXECUTE FUNCTIONS

// test load logic of sys array controller
// by mocking on-chip memory response to requests from sys array controller
// and mocking on-chip memory inputs to sys array controller
int complete_load(int& tickcount, Vsys_array_controller* tb, VerilatedVcdC* tfp,
                    int index, std::vector<std::vector<int>>& B) {

    int cycle_count = 0;
    int max_cycle_count = MESHUNITS * (TILEUNITS + 1) + 10;
    int addr = index == 0 ? b0_addr : b1_addr;
    char err_msg[100];
    while (true) {
        for (int i = 0; i < MESHUNITS; i++) {
            // mock memory ignores invalid sys array requests
            if (!tb->B_read_valid[i]) {
                continue;
            }

            // assert sys array memory address request matches a word within the
            // block of address previously provided to sys array loader
            int mesh_addr = tb->B_col_read_addrs[i];
            sprintf(err_msg, "Invalid B mesh addr: expected (within) %d, actual=%d", addr, mesh_addr);
            condition_err(err_msg, mesh_addr < addr || mesh_addr >= addr + MATSIZE);

            // mock memory responds to memory request with the 
            // corresponding word in B
            // * words in B are stored in row-major order
            int row = (mesh_addr - addr) / (MESHUNITS * TILEUNITS);
            int col = (mesh_addr - addr) % (MESHUNITS * TILEUNITS);
            for (int j = 0; j < TILEUNITS; j++) {
                tb->B[i][j] = B[row][col + j];
            }
        }
        tick(tickcount, tb, tfp);
        cycle_count++;
        if (tb->load_finished) {
            break;
        }
        condition_err("Timed out waiting for load to complete", cycle_count >= max_cycle_count);
    }

    // assert the total load took MESHUNITS * (TILEUNITS + 1) cycles
    // see `sys_array_controller.v` for calculation of total cycles
    sprintf(err_msg, "Incorrect load cycles: expected=%d, actual=%d", MESHUNITS * (TILEUNITS + 1), cycle_count);
    condition_err(err_msg, return cycle_count != MESHUNITS * (TILEUNITS + 1));
    
    tick(tickcount, tb, tfp);
    signal_err("tb->load_lock_res", 0, tb->load_lock_res[index]);
    return SUCCESS;
}

// test compute logic of sys array controller
// by mocking on-chip memory response to requests from sys array controller
// and mocking on-chip memory inputs to sys array controller
int complete_comp(int& tickcount, Vsys_array_controller* tb, VerilatedVcdC* tfp, int index,
                    std::vector<std::vector<int>>& A, std::vector<std::vector<int>>& D,
                    std::vector<std::vector<int>>& C, std::vector<std::vector<int>>& expected_C) {
    
    int cycle_count = 0;
    int max_cycle_count = MESHUNITS * (TILEUNITS + 2) + 10;
    char err_msg[100];
    while (true) {
        for (int i = 0; i < MESHUNITS; i++) {
            if (tb->A_read_valid[i]) {
                // assert sys array memory address request matches a word within the
                // block of address previously provided to sys array loader
                int A_mesh_addr = tb->A_row_read_addrs[i];
                sprintf(err_msg, "Invalid A mesh addr: expected (within) %d, actual=%d", a_addr, A_mesh_addr);
                condition_err(err_msg, A_mesh_addr < a_addr && A_mesh_addr >= a_addr + MATSIZE);

                // mock memory responds to memory request with the 
                // corresponding word in A
                // * words are stored in row-major order
                int A_row = (A_mesh_addr - a_addr) / (MESHUNITS * TILEUNITS);
                int A_col = (A_mesh_addr - a_addr) % (MESHUNITS * TILEUNITS);
                for (int j = 0; j < TILEUNITS; j++) {
                    tb->A[i][j] = A[A_row][A_col + j];
                }
            }
            if (tb->D_read_valid[i]) {
                // assert sys array memory address request matches a word within the
                // block of address previously provided to sys array loader
                int D_mesh_addr = tb->D_col_read_addrs[i];
                sprintf(err_msg, "Invalid D mesh addr: expected (within) %d, actual=%d", d_addr, D_mesh_addr);
                condition_err(err_msg, D_mesh_addr < d_addr && D_mesh_addr >= d_addr + MATSIZE);

                // mock memory responds to memory request with the 
                // corresponding word in D
                // * words are stored in row-major order
                int D_row = (D_mesh_addr - d_addr) / (MESHUNITS * TILEUNITS);
                int D_col = (D_mesh_addr - d_addr) % (MESHUNITS * TILEUNITS);
                for (int j = 0; j < TILEUNITS; j++) {
                    tb->D[i][j] = D[D_row][D_col + j];
                }
            }
        }
        tick(tickcount, tb, tfp);
        cycle_count++;
        if (tb->comp_finished) {
            break;
        }
        condition_err("Timed out waiting for comp to complete", cycle_count >= max_cycle_count);

        
        for (int i = 0; i < MESHUNITS; i++) {
            // mock memory ignores invalid sys array requests
            if (!tb->C_write_valid[i]) {
                continue;
            }

            // assert sys array memory address request matches a word within the
            // block of address previously provided to sys array loader
            int C_mesh_addr = tb->C_col_write_addrs[i];
            sprintf(err_msg, "Invalid C mesh addr: expected (within) %d, actual=%d", c_addr, C_mesh_addr);
            condition_err(err_msg, C_mesh_addr < c_addr && C_mesh_addr >= c_addr + MATSIZE);
            
            // mock memory records the word written to the C address
            // by the sys array controller
            int C_row = (C_mesh_addr - c_addr) / (MESHUNITS * TILEUNITS);
            int C_col = (C_mesh_addr - c_addr) % (MESHUNITS * TILEUNITS);
            for (int j = 0; j < TILEUNITS; j++) {
                C[C_row][C_col + j] = tb->C[i][j];
            }
        }
    }

    // assert the total comp took MESHUNITS * (TILEUNITS + 2) cycles
    // see `sys_array_controller.v` for calculation of total cycles
    sprintf(err_msg, "Incorrect comp cycles: expected=%d, actual=%d", MESHUNITS * (TILEUNITS + 2), cycle_count);
    condition_err(err_msg, cycle_count != MESHUNITS * (TILEUNITS + 2) - 1);

    // assert sys array writes the correct values to C in mock memory
    for (int i = 0; i < MESHUNITS * TILEUNITS; i++) {
        for (int j = 0; j < MESHUNITS * TILEUNITS; j++) {
            sprintf(err_msg, "C[%d][%d]", i, j);
            data_err(err_msg, expected_C[i][j], C[i][j]);
        }
    }

    tick(tickcount, tb, tfp);
    signal_err("tb->comp_lock_res", 0, tb->comp_lock_res[index]);
    return SUCCESS;
}

// test simultaneous compute and load logic of sys array controller
// by mocking on-chip memory response to requests from sys array controller
// and mocking on-chip memory inputs to sys array controller
int complete_load_and_comp(int& tickcount, Vsys_array_controller* tb, VerilatedVcdC* tfp, int load_idx,
                    std::vector<std::vector<int>>& B,
                    std::vector<std::vector<int>>& A, std::vector<std::vector<int>>& D,
                    std::vector<std::vector<int>>& C, std::vector<std::vector<int>>& expected_C) {

    int comp_idx = 1 - load_idx;
    int cycle_count = 0;
    int max_cycle_count = MESHUNITS * (TILEUNITS + 2) + 10;
    int b_addr = load_idx == 0 ? b0_addr : b1_addr;
    bool load_finished = false;
    bool comp_finished = false;
    char err_msg[100];
    while (true) {
        for (int i = 0; i < MESHUNITS; i++) {
            // for each input matrix (A, B, D):
            // i. verify address requested by sys array controller is expected
            // ii. respond to sys array request with mock memory word
            if (tb->A_read_valid[i]) {
                int A_mesh_addr = tb->A_row_read_addrs[i];
                sprintf(err_msg, "Invalid A mesh addr: expected (within) %d, actual=%d", a_addr, A_mesh_addr);
                condition_err(err_msg, A_mesh_addr < a_addr && A_mesh_addr >= a_addr + MATSIZE);
                int A_row = (A_mesh_addr - a_addr) / (MESHUNITS * TILEUNITS);
                int A_col = (A_mesh_addr - a_addr) % (MESHUNITS * TILEUNITS);
                for (int j = 0; j < TILEUNITS; j++) {
                    tb->A[i][j] = A[A_row][A_col + j];
                }
            }
            if (tb->D_read_valid[i]) {
                int D_mesh_addr = tb->D_col_read_addrs[i];
                sprintf(err_msg, "Invalid D mesh addr: expected (within) %d, actual=%d", d_addr, D_mesh_addr);
                condition_err(err_msg, D_mesh_addr < d_addr && D_mesh_addr >= d_addr + MATSIZE);
                int D_row = (D_mesh_addr - d_addr) / (MESHUNITS * TILEUNITS);
                int D_col = (D_mesh_addr - d_addr) % (MESHUNITS * TILEUNITS);
                for (int j = 0; j < TILEUNITS; j++) {
                    tb->D[i][j] = D[D_row][D_col + j];
                }
            }
            if (tb->B_read_valid[i]) {
                int mesh_addr = tb->B_col_read_addrs[i];
                sprintf(err_msg, "Invalid B mesh addr: expected (within) %d, actual=%d", b_addr, mesh_addr);
                condition_err(err_msg, mesh_addr < b_addr && mesh_addr >= b_addr + MATSIZE);
                int row = (mesh_addr - b_addr) / (MESHUNITS * TILEUNITS);
                int col = (mesh_addr - b_addr) % (MESHUNITS * TILEUNITS);
                for (int j = 0; j < TILEUNITS; j++) {
                    tb->B[i][j] = B[row][col + j];
                }
            }
        }
        tick(tickcount, tb, tfp);
        cycle_count++;
        if (tb->comp_finished) {
            comp_finished = true;
        }
        if (tb->load_finished) {
            load_finished = true;
        }
        if (comp_finished && load_finished) {
            break;
        }
        condition_err("Timed out waiting for load/comp to complete", cycle_count >= max_cycle_count);

        // i. verify address requested by sys array controller for output matrix (C) is expected
        // ii. store output word 
        for (int i = 0; i < MESHUNITS; i++) {
            if (!tb->C_write_valid[i]) {
                continue;
            }
            int C_mesh_addr = tb->C_col_write_addrs[i];
            sprintf(err_msg, "Invalid B mesh addr: expected (within) %d, actual=%d", c_addr, C_mesh_addr);
            condition_err(err_msg, C_mesh_addr < c_addr && C_mesh_addr >= c_addr + MATSIZE);
            int C_row = (C_mesh_addr - c_addr) / (MESHUNITS * TILEUNITS);
            int C_col = (C_mesh_addr - c_addr) % (MESHUNITS * TILEUNITS);
            for (int j = 0; j < TILEUNITS; j++) {
                C[C_row][C_col + j] = tb->C[i][j];
            }
        }
    }

    // assert sys array writes the correct values to C in mock memory
    for (int i = 0; i < MESHUNITS * TILEUNITS; i++) {
        for (int j = 0; j < MESHUNITS * TILEUNITS; j++) {
            sprintf(err_msg, "C[%d][%d]", i, j);
            data_err(err_msg, expected_C[i][j], C[i][j]);
        }
    }
    tick(tickcount, tb, tfp);
    signal_err("tb->comp_lock_res", 0, tb->comp_lock_res[comp_idx]);
    signal_err("tb->load_lock_res", 0, tb->load_lock_res[load_idx]);
    return SUCCESS;
}


int main(int argc, char** argv) {

    Verilated::commandArgs(argc, argv);
    Vsys_array_controller* tb = new Vsys_array_controller;
    Verilated::traceEverOn(true);
    
    int tickcount = 0;
    VerilatedVcdC* tfp = new VerilatedVcdC;
    tb->trace(tfp, 99);
    tfp->open("sys_array_ctrl.vcd");

    // SETUP MOCK MEMORY
    std::vector<std::vector<int>> B0(MESHUNITS * TILEUNITS, std::vector<int>(MESHUNITS * TILEUNITS));
    std::vector<std::vector<int>> B1(MESHUNITS * TILEUNITS, std::vector<int>(MESHUNITS * TILEUNITS));
    std::vector<std::vector<int>> A(MESHUNITS * TILEUNITS, std::vector<int>(MESHUNITS * TILEUNITS));
    std::vector<std::vector<int>> D(MESHUNITS * TILEUNITS, std::vector<int>(MESHUNITS * TILEUNITS));
    for (int i = 0; i < MESHUNITS * TILEUNITS; i++) {
        for (int j = 0; j < MESHUNITS * TILEUNITS; j++) {
            B0[i][j] = rand() % MAX_INP;
            B1[i][j] = rand() % MAX_INP;
            A[i][j] = rand() % MAX_INP;
            D[i][j] = rand() % MAX_INP;
        }
    }
    std::vector<std::vector<int>> C(MESHUNITS * TILEUNITS, std::vector<int>(MESHUNITS * TILEUNITS));
    std::vector<std::vector<int>> expected_C0 (MESHUNITS * TILEUNITS, std::vector<int>(MESHUNITS * TILEUNITS));
    std::vector<std::vector<int>> expected_C1 (MESHUNITS * TILEUNITS, std::vector<int>(MESHUNITS * TILEUNITS));
    for (int i = 0; i < MESHUNITS * TILEUNITS; i++) {
        for (int j = 0; j < MESHUNITS * TILEUNITS; j++) {
            expected_C0[i][j] = D[i][j];
            expected_C1[i][j] = D[i][j];
            for (int k = 0; k < MESHUNITS * TILEUNITS; k++) {
                expected_C0[i][j] += A[i][k] * B0[k][j];
                expected_C1[i][j] += A[i][k] * B1[k][j];
            }
        }
    }

    init(tickcount, tb, tfp);

    // TEST 1: single-threaded load, single-threaded comp
    test_runner("[SYS ARRAY CTRL]", "ST LOAD + ST COMP", 
        [&tickcount, &tb, &tfp, &B0, &A, &D, &C, &expected_C0](){
            double_load_req_conflict(tickcount, tb, tfp);
            complete_load(tickcount, tb, tfp, 0, B0);
            double_comp_req_conflict(tickcount, tb, tfp);
            complete_comp(tickcount, tb, tfp, 0, A, D, C, expected_C0);
        },
        [&tfp](){
            tfp->close();
        });

    // TEST 2: two single-threaded loads, two single-threaded comps
    test_runner("[SYS ARRAY CTRL]", "ST LOAD + ST LOAD + ST COMP", 
        [&tickcount, &tb, &tfp, &B0, &B1, &A, &D, &C, &expected_C0, &expected_C1](){
            single_load_req(tickcount, tb, tfp, 0);
            complete_load(tickcount, tb, tfp, 0, B0);
            single_load_req(tickcount, tb, tfp, 1);
            complete_load(tickcount, tb, tfp, 1, B1);
            single_comp_req(tickcount, tb, tfp, 0);
            complete_comp(tickcount, tb, tfp, 0, A, D, C, expected_C0);
            single_comp_req(tickcount, tb, tfp, 1);
            complete_comp(tickcount, tb, tfp, 1, A, D, C, expected_C1);
        },
        [&tfp](){
            tfp->close();
        });

    // TEST 3: single-threaded load, double-threaded comp + load, single-threaded comp
    test_runner("[SYS ARRAY CTRL]", "ST LOAD + DT COMP/LOAD + ST COMP", 
        [&tickcount, &tb, &tfp, &B0, &B1, &A, &D, &C, &expected_C0, &expected_C1](){
            single_load_req(tickcount, tb, tfp, 0);
            complete_load(tickcount, tb, tfp, 0, B0);
            load_and_comp_req_no_conflict(tickcount, tb, tfp, 1);
            complete_load_and_comp(tickcount, tb, tfp, 1, B1, A, D, C, expected_C0);
            single_comp_req(tickcount, tb, tfp, 1);
            complete_comp(tickcount, tb, tfp, 1, A, D, C, expected_C1);
        },
        [&tfp](){
            tfp->close();
        });
    printf("All tests passed\n");
    tfp->close();
}
