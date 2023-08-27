#include <stdio.h>
#include <stdlib.h>
#include "Vsys_array.h"
#include "verilated.h"
#include "verilated_vcd_c.h"

#define SUCCESS
#define SIM_ERROR
#define INTERNAL_ERROR

void tick(int tickcount, Vsys_array* tb, VerilatedVcdC* tfp) {
    tb->eval();
    if (tfp)
        tfp->dump(tickcount * 10 - 2);
    tb->clock = 1;
    tb->eval();
    if (tfp)
        tfp->dump(tickcount * 10);
    tb->clock = 0;
    tb->eval();
    if (tfp)
        tfp->dump(tickcount * 10 + 5);
}

int basic_matmul(Vsys_array* tb, VerilatedVcdC* tfp, 
        unsigned int meshcols, unsigned int meshrows,
        unsigned int tilecols, unsigned int tilerows,
        int** A, int** B, int** D, int** expected_C) {
    int tickcount = 0;

    // init
    tb->reset = 1;
    tick(tickcount, tb, tfp);
    tb->reset = 0;
    tick(tickcount, tb, tfp);

    // propagate B through array
    for (int row = 0; row < meshrows; row++) {
        tick(tickcount, tb, tfp);
        for (int m_col = 0; m_col < meshcols; m_col++) {
            for (int t_col = 0; t_col < tilecols; t_col++) {
                tb->in_b[m_col][t_col] = B[row][m_col * tilecols + t_col];
                tb->in_propagate[m_col][t_col] = 0;
                tb->in_valid[m_col][t_col] = 1;
            }
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 5) {
        printf("Provide args: [BITWIDTH] [MESHCOLUMNS] [MESHROWS] [TILECOLUMNS] [TILEROWS]");
        return INTERNAL_ERROR;
    }
    unsigned int bitwidth = std::stoi(argv[0]);
    unsigned int meshcols = std::stoi(argv[1]);
    unsigned int meshrows = std::stoi(argv[2]);
    unsigned int tilecols = std::stoi(argv[3]);
    unsigned int tilerows = std::stoi(argv[4]);

    Verilated::commandArgs(argc, argv);
    Vsys_array* tb = new Vsys_array;
    Verilated::traceEverOn(true);
    VerilatedVcdC* tfp = new VerilatedVcdC;
    tb->trace(tfp, 99);
    tfp->open("sys_array.vcd");

    int tickcount = 0;

    return 0;
}