#include "utils.h"

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

// DUMMY ADDRESSES FOR B0 AND B1
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


// LOAD FUNCTIONS

int double_load_req(int& tickcount, Vsys_array_controller* tb, VerilatedVcdC* tfp) {
    tb->load_lock_req[0] = 1;
    tb->load_lock_req[1] = 1;
    tb->comp_lock_req[0] = 0;
    tb->comp_lock_req[1] = 0;
    tb->B_addr[0] = b0_addr;
    tb->B_addr[1] = b1_addr;
    tick(tickcount, tb, tfp);
    tb->load_lock_req[0] = 0;
    tb->load_lock_req[1] = 0;
    if (!tb->load_lock_res[0] || tb->load_lock_res[1] || tb->comp_lock_res[0] || tb->comp_lock_res[1]) {
        return SIM_ERROR;
    }
    return SUCCESS;
}

int complete_load(int& tickcount, Vsys_array_controller* tb, VerilatedVcdC* tfp,
                    int index, std::vector<std::vector<int>>& B) {

    int cycle_count = 0;
    int max_cycles = 1000;
    int addr = index == 0 ? b0_addr : b1_addr;
    while (true) {
        for (int i = 0; i < MESHUNITS; i++) {
            if (!tb->B_read_valid[i]) {
                continue;
            }
            int mesh_addr = tb->B_col_read_addrs[i];
            if (mesh_addr < addr || mesh_addr >= addr + MATSIZE) {
                return SIM_ERROR;
            }
            int row = mesh_addr / (MESHUNITS * TILEUNITS);
            int col = mesh_addr % (MESHUNITS * TILEUNITS);
            for (int j = 0; j < TILEUNITS; j++) {
                tb->B[i][j] = B[row][col + j];
            }
        }
        tick(tickcount, tb, tfp);
        cycle_count++;
        if (tb->load_finished) {
            break;
        }
        if (cycle_count >= max_cycles) {
            return SIM_ERROR;
        }
    }
    if (cycle_count != MESHUNITS * (TILEUNITS + 1)) {
        return SIM_ERROR;
    }
    
    tick(tickcount, tb, tfp);
    if (tb->load_lock_res[index]) {
        return SIM_ERROR;
    }

    return SUCCESS;
}

// COMP FUNCTIONS

int double_comp_req(int& tickcount, Vsys_array_controller* tb, VerilatedVcdC* tfp) {
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
    if (tb->load_lock_res[0] || tb->load_lock_res[1] || !tb->comp_lock_res[0] || tb->comp_lock_res[1]) {
        return SIM_ERROR;
    }
    return SUCCESS;
}

int complete_comp(int& tickcount, Vsys_array_controller* tb, VerilatedVcdC* tfp, 
                    std::vector<std::vector<int>>& A, std::vector<std::vector<int>>& D,
                    std::vector<std::vector<int>>& C, std::vector<std::vector<int>>& expected_C) {
    int a;
    int cycle_count = 0;
    int max_cycles = 1000;
    while (true) {
        for (int i = 0; i < MESHUNITS; i++) {
            if (tb->A_read_valid[i]) {
                int A_mesh_addr = tb->A_row_read_addrs[i];
                if (A_mesh_addr < a_addr || A_mesh_addr >= a_addr + MATSIZE) {
                    return SIM_ERROR;
                }
                int A_row = (A_mesh_addr - a_addr) / (MESHUNITS * TILEUNITS);
                int A_col = (A_mesh_addr - a_addr) % (MESHUNITS * TILEUNITS);
                for (int j = 0; j < TILEUNITS; j++) {
                    tb->A[i][j] = A[A_row][A_col + j];
                }
            }
            if (tb->D_read_valid[i]) {
                int D_mesh_addr = tb->D_col_read_addrs[i];
                if (D_mesh_addr < d_addr || D_mesh_addr >= d_addr + MATSIZE) {
                    return SIM_ERROR;
                }
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
        if (cycle_count >= max_cycles) {
            return SIM_ERROR;
        }
        for (int i = 0; i < MESHUNITS; i++) {
            if (!tb->C_write_valid[i]) {
                continue;
            }
            int C_mesh_addr = tb->C_col_write_addrs[i];
            if (C_mesh_addr < c_addr || C_mesh_addr >= c_addr + MATSIZE) {
                return SIM_ERROR;
            }
            int C_row = (C_mesh_addr - c_addr) / (MESHUNITS * TILEUNITS);
            int C_col = (C_mesh_addr - c_addr) % (MESHUNITS * TILEUNITS);
            for (int j = 0; j < TILEUNITS; j++) {
                C[C_row][C_col + j] = tb->C[i][j];
            }
        }
    }

    for (int i = 0; i < MESHUNITS * TILEUNITS; i++) {
        for (int j = 0; j < MESHUNITS * TILEUNITS; j++) {
            if (C[i][j] != expected_C[i][j]) {
                return SIM_ERROR;
            }
        }
    }
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

    int res;
    init(tickcount, tb, tfp);
    res = double_load_req(tickcount, tb, tfp);
    if (res != SUCCESS) {
        printf("Double load request failed: %d\n", res);
        tfp->close();
        return 0;
    }

    std::vector<std::vector<int>> B(MESHUNITS * TILEUNITS, std::vector<int>(MESHUNITS * TILEUNITS));
    for (int i = 0; i < MESHUNITS * TILEUNITS; i++) {
        for (int j = 0; j < MESHUNITS * TILEUNITS; j++) {
            B[i][j] = i == j ? 1 : 0;
        }
    }
    res = complete_load(tickcount, tb, tfp, 0, B);
    if (res != SUCCESS) {
        printf("Load complete failed: %d\n", res);
        tfp->close();
        return 0;
    }

    res = double_comp_req(tickcount, tb, tfp);
    if (res != SUCCESS) {
        printf("Double comp request failed: %d\n", res);
        tfp->close();
        return 0;
    }

    std::vector<std::vector<int>> A(MESHUNITS * TILEUNITS, std::vector<int>(MESHUNITS * TILEUNITS));
    std::vector<std::vector<int>> D(MESHUNITS * TILEUNITS, std::vector<int>(MESHUNITS * TILEUNITS));
    std::vector<std::vector<int>> expected_C (MESHUNITS * TILEUNITS, std::vector<int>(MESHUNITS * TILEUNITS));
    for (int i = 0; i < MESHUNITS * TILEUNITS; i++) {
        for (int j = 0; j < MESHUNITS * TILEUNITS; j++) {
            A[i][j] = rand() % MAX_INP;
            D[i][j] = rand() % MAX_INP;
            expected_C[i][j] = D[i][j];
            for (int k = 0; k < MESHUNITS * TILEUNITS; k++) {
                expected_C[i][j] += A[i][k] * B[k][j];
            }
        }
    }
    std::vector<std::vector<int>> C(MESHUNITS * TILEUNITS, std::vector<int>(MESHUNITS * TILEUNITS));
    res = complete_comp(tickcount, tb, tfp, A, D, C, expected_C);
    if (res != SUCCESS) {
        printf("Comp complete failed\n");
        tfp->close();
        return 0;
    }


    printf("Controller test fully passed\n");
    tfp->close();
}
