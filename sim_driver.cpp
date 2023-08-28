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

    // propagate A and D through array and collect C
    int a_outer_rows = meshcols;
    int a_outer_cols = meshrows;
    int a_inner_rows = tilecols;
    int a_inner_cols = tilerows;
    int d_outer_rows = meshcols;
    int d_outer_cols = meshcols;
    int d_inner_rows = tilecols;
    int d_inner_cols = tilecols;
    int max_outer_idx_sum = meshcols < meshrows 
                                ? a_outer_cols + a_outer_rows - 2
                                : 2 * d_outer_cols - 2;
    
    // go through each set of blocks with index [i, j] s.t. i + j = 0, 1, 2, ...
    for (int outer_idx_sum = 0; outer_idx_sum <= max_outer_idx_sum; outer_idx_sum++) {
        // go through the inner row of each block
        // note: by construction A and D must have the same number of inner rows per block
        // note: we must use the inner row in the outer loop here because each row (over all blocks in the set)
        //     must be sent in the same clock cycle
        for (int inner_row = 0; inner_row < tilecols; inner_row++) {
            tick(tickcount, tb, tfp);
            
            // go through each block in the set (verify its correctness) and send to the array
            for (int outer_row = 0; outer_row <= outer_idx_sum; outer_row++) {
                int outer_col = outer_idx_sum - outer_row;
                if (outer_row < a_outer_rows && outer_col < a_outer_cols) {
                    for (int inner_col = 0; inner_col < a_inner_cols; inner_col++) {
                        tb->in_a[outer_col][inner_col] = A[outer_row * a_inner_rows + inner_row][outer_col * a_inner_cols + inner_col];
                    }
                }
                if (outer_row < d_outer_rows && outer_col < d_outer_cols) {
                    for (int inner_col = 0; inner_col < d_inner_cols; inner_col++) {
                        tb->in_d[outer_col][inner_col] = D[outer_row * d_inner_rows + inner_row][outer_col * d_inner_cols + inner_col];
                    }
                }
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