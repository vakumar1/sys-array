#include "matrix_state.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include "Vsys_array.h"
#include "verilated.h"
#include "verilated_vcd_c.h"
#include <vector>
#include <string>

// DEFAULT MESH PARAMETERS: 256 mesh
#ifndef BITWIDTH
#define BITWIDTH 32
#endif

#ifndef MESHCOLS
#define MESHCOLS 4
#endif

#ifndef TILECOLS
#define TILECOLS 4
#endif

#ifndef MESHROWS
#define MESHROWS 4
#endif

#ifndef TILEROWS 
#define TILEROWS 4
#endif

// INPUT MATRIX ENTRY MAX
#define MAX_INP (1 << ((BITWIDTH / 2) - 2)) / (MESHROWS * TILEROWS)

void tick(int& tickcount, Vsys_array* tb, VerilatedVcdC* tfp) {
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

int multi_matmul(int& tickcount, Vsys_array* tb, VerilatedVcdC* tfp, int num_mats, std::vector<int> c_rows,
                    std::vector<std::vector<std::vector<int>>>& A, std::vector<std::vector<std::vector<int>>>& B,
                    std::vector<std::vector<std::vector<int>>>& D, std::vector<std::vector<std::vector<int>>>& expected_C) {

     // init
    tb->reset = 1;
    tb->in_dataflow = 1;
    tick(tickcount, tb, tfp);
    tb->reset = 0;
    tick(tickcount, tb, tfp);

    unsigned char propagate = 0;
    for (int i = 0; i <= num_mats; i++) {
        int collecting_idx = i - 1;
        int loading_idx = i;
        bool ignore_collecting = collecting_idx < 0;
        bool ignore_loading = loading_idx >= num_mats;

        feeding_mat_state A_state = feeding_mat_state(MESHROWS, TILEROWS);
        feeding_mat_state B_state = feeding_mat_state(MESHCOLS, TILECOLS);
        reading_mat_state C_state = reading_mat_state(MESHCOLS, TILECOLS);
        feeding_mat_state D_state = feeding_mat_state(MESHCOLS, TILECOLS);

        std::vector<std::vector<int>> A_mat;
        std::vector<std::vector<int>> B_mat;
        std::vector<std::vector<int>> C_mat;
        std::vector<std::vector<int>> D_mat;

        int collecting_c_rows = ignore_collecting ? 0 : c_rows[collecting_idx];
        if (collecting_idx >= 0) {
            A_state = feeding_mat_state(MESHROWS, TILEROWS, collecting_c_rows, true);
            C_state = reading_mat_state(MESHCOLS, TILECOLS, collecting_c_rows, true);
            D_state = feeding_mat_state(MESHCOLS, TILECOLS, collecting_c_rows, true);
            A_mat = A[collecting_idx];
            C_mat = std::vector<std::vector<int>>(collecting_c_rows, std::vector<int>(MESHCOLS * TILECOLS));
            D_mat = D[collecting_idx];
        }

        if (loading_idx < num_mats) {
            B_state = feeding_mat_state(MESHCOLS, TILECOLS, MESHROWS * TILEROWS, false);
            B_mat = B[loading_idx];
        }

        A_state.start();
        B_state.start();
        C_state.start();
        D_state.start();
        int waiting_iters = 0;
        while (true) {
            tick(tickcount, tb, tfp);
            
            // propagate B signals
            for (int i = 0; i < MESHCOLS; i++) {
                for (int j = 0; j < TILECOLS; j++) {
                    tb->in_propagate[i][j] = propagate;
                    tb->in_b_shelf_life[i][j] = B_state.current_row(i) + 1;
                }
            }

            // propagate inputs and read from output
            bool a_done = A_state.update<MESHROWS * TILEROWS, MESHROWS, TILEROWS>(collecting_c_rows, A_mat, tb->in_a, tb->in_a_valid);
            bool b_done = B_state.update<MESHCOLS * TILECOLS, MESHCOLS, TILECOLS>(MESHROWS * TILEROWS, B_mat, tb->in_b, tb->in_b_valid);
            bool c_done = C_state.update<MESHCOLS * TILECOLS, MESHCOLS, TILECOLS>(collecting_c_rows, tb->out_c, tb->out_c_valid, C_mat);
            bool d_done = D_state.update<MESHCOLS * TILECOLS, MESHCOLS, TILECOLS>(collecting_c_rows, D_mat, tb->in_d, tb->in_d_valid);
            bool c_valid = C_state.valid();

            // wait until all inputs and outputs are done
            signal_err("c_valid", c_valid, 1);
            condition_err("Timed out waiting for C output", [waiting_iters](){ return waiting_iters >= 10; });
            if ((b_done && a_done && d_done)) {
                waiting_iters++;
                if (c_done) {
                    break;
                }
            }
        }
        // alternate propagate for next iteration
        propagate = !propagate;
        
        // verify output correctness
        if (ignore_collecting) {
            continue;
        }
        char C_msg[100];
        for (int i = 0; i < collecting_c_rows; i++) {
            for (int j = 0; j < MESHCOLS * TILECOLS; j++) {
                sprintf(C_msg, "C[%d][%d][%d]", collecting_idx, i, j);
                data_err(C_msg, expected_C[collecting_idx][i][j], C_mat[i][j]);
            }
        }
    }
    return SUCCESS;
}

int main(int argc, char** argv) {

    Verilated::commandArgs(argc, argv);
    Vsys_array* tb = new Vsys_array;
    Verilated::traceEverOn(true);
    
    int tickcount = 0;
    VerilatedVcdC* tfp = new VerilatedVcdC;
    tb->trace(tfp, 99);
    tfp->open("sys_array.vcd");

    // number of matrices to run
    int num_mats = 1;
    int height = MESHROWS * TILEROWS;
    bool random = false;
    bool identity = false;
    bool affine = false;
    bool negative = false;
    
    for (int i = 1; i < argc; i++) {
        std::string flag = argv[i];
        if (flag == "--num-mats") {
            num_mats = std::stoi(argv[i + 1]);
            i++;
        } else if (flag == "--height") {
            height = std::stoi(argv[i + 1]);
            i++;
        } else if (flag == "--random") {
            random = true;
        } else if (flag == "--identity") {
            identity = true;
        } else if (flag == "--affine") {
            affine = true;
        } else if (flag == "--negative") {
            negative = true;
        }
    }

    std::vector<int> c_rows_s;
    std::vector<std::vector<std::vector<int>>> As;
    std::vector<std::vector<std::vector<int>>> Bs;
    std::vector<std::vector<std::vector<int>>> Ds;
    std::vector<std::vector<std::vector<int>>> expected_Cs;
    for (int test = 0; test < num_mats; test++) {
        int c_rows = height;
        c_rows_s.push_back(c_rows);
        std::vector<std::vector<int>> A(c_rows, std::vector<int>(MESHROWS * TILEROWS));
        for (int i = 0; i < c_rows; i++) {
            for (int j = 0; j < MESHROWS * TILEROWS; j++) {
                if (random) {
                    A[i][j] = rand() % MAX_INP;
                } else {
                    A[i][j] = i + j;
                }
                if (negative) {
                    A[i][j] = -A[i][j];
                }
            }
        }

        std::vector<std::vector<int>> B(MESHROWS * TILEROWS, std::vector<int>(MESHCOLS * TILECOLS));
        for (int i = 0; i < MESHROWS * TILEROWS; i++) {
            for (int j = 0; j < MESHCOLS * TILECOLS; j++) {
                if (identity) {
                    B[i][j] = (i == j) ? 1 : 0;
                } else {
                    B[i][j] = rand() % MAX_INP;
                }
            }
        }
        
        std::vector<std::vector<int>> expected_C(c_rows, std::vector<int>(MESHCOLS * TILECOLS));
        std::vector<std::vector<int>> D(c_rows, std::vector<int>(MESHCOLS * TILECOLS));

        for (int i = 0; i < c_rows; i++) {
            for (int j = 0; j < MESHCOLS * TILECOLS; j++) {
                if (affine) {
                    D[i][j] = rand() % MAX_INP;
                } else {
                    D[i][j] = 0;
                }
                expected_C[i][j] = D[i][j];
                for (int k = 0; k < MESHROWS * TILEROWS; k++) {
                    expected_C[i][j] += A[i][k] * B[k][j];
                }
            }
        }
        As.push_back(A);
        Bs.push_back(B);
        Ds.push_back(D);
        expected_Cs.push_back(expected_C);
    }


    void test_runner(VerilatedVcdC* tfp, std::string test_header, std::string test_name, std::function<void()> test);

    char matmul_test_name[100];
    sprintf(matmul_test_name, "MULTI MATMUL: num_mats=%d height=%d rand=%d id=%d aff=%d neg=%d",
            num_mats, height, random, identity, affine, negative);
    test_runner(tfp, "[SYS ARRAY]", matmul_test_name, 
            [&tickcount, &tb, &tfp, num_mats, c_rows_s, &As, &Bs, &Ds, &expected_Cs](){ multi_matmul(tickcount, tb, tfp, num_mats, c_rows_s, As, Bs, Ds, expected_Cs); });
    printf("All tests passed\n");
    tfp->close();
    return 0;
}
