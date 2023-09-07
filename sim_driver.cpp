#include <stdio.h>
#include <stdlib.h>
#include "Vsys_array.h"
#include "verilated.h"
#include "verilated_vcd_c.h"

#define SUCCESS
#define SIM_ERROR
#define INTERNAL_ERROR

enum mat_stage {
    WAITING,
    PROCESSING,
    DONE
};

class mat_state {
private:
    int mesh_size;
    int tile_size;
    int mat_rows;
    std::vector<mat_stage> mesh_unit_state;
    std::vector<int> mesh_unit_row;
    bool ordered_rows;
public:
    mat_state(int mesh_size, int tile_size, int mat_rows, bool ordered_rows) {
        this->mesh_size = mesh_size;
        this->tile_size = tile_size;
        this->mat_rows = mat_rows;
        this->mesh_unit_state.resize(mesh_size);
        this->mesh_unit_row.resize(mesh_size);
        this->ordered_rows = ordered_rows;
    }

    // init state and kickoff feeding
    void start() {
        int start_row = this->ordered_rows ? 0 : mat_rows - 1;
        for (int i = 0; i < mesh_size; i++) {
            this->mesh_unit_state.push_back(WAITING);
            this->mesh_unit_row.push_back(start_row);
        }
        this->mesh_unit_state[0] = PROCESSING;
    }

    // update the internal feed/read state by one tick
    bool update(int** in_mat, int** out_mat, int** out_valid) {
        mat_stage next_mesh_unit_state[mesh_size];
        for (int mesh_unit = 0; mesh_unit < mesh_size; mesh_unit++) {
            switch (this->mesh_unit_state[mesh_unit]) {
            case WAITING:
                // feed in invalid signal to the array for each tile unit
                for (int tile_unit = 0; tile_unit < tile_size; tile_unit++) {
                    out_valid[mesh_unit][tile_unit] = 0;
                }

                // update feeding to true if the previous mesh unit is feeding/done
                if (mesh_unit > 0 && this->mesh_unit_state[mesh_unit - 1] != WAITING) {
                    next_mesh_unit_state[mesh_unit] = PROCESSING;
                }
                break;

            case PROCESSING:
                // feed in the input matrix (+ valid signal) to the array for each tile row in given mesh_row
                for (int tile_unit = 0; tile_unit < tile_size; tile_unit++) {
                    out_mat[mesh_unit][tile_unit] = in_mat[this->mesh_unit_row[mesh_unit]][mesh_unit * mesh_size + tile_unit];
                    out_valid[mesh_unit][tile_unit] = 1;
                }

                // incr input matrix row (and end feeding if complete)
                if (this->ordered_rows) {
                    this->mesh_unit_row[mesh_unit]++;
                    if (this->mesh_unit_row[mesh_unit] >= mat_rows) {
                        next_mesh_unit_state[mesh_unit] = DONE;
                    }
                } else {
                    this->mesh_unit_row[mesh_unit]--;
                    if (this->mesh_unit_row[mesh_unit] < 0) {
                        next_mesh_unit_state[mesh_unit] = DONE;
                    }
                }
                break;

            case DONE:
                // feed in invalid signal to the array for each tile row
                for (int tile_unit = 0; tile_unit < tile_size; tile_unit++) {
                    out_valid[mesh_unit][tile_unit] = 0;
                }
                break;
            }
        }

        for (int mesh_unit = 0; mesh_unit < mesh_size; mesh_unit++) {
            this->mesh_unit_state[mesh_unit] = next_mesh_unit_state[mesh_unit];
        }

        // feeding is complete once the last mesh_row has finished
        return this->mesh_unit_state[mesh_size - 1] == DONE;
    }
};


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
        const int meshcols, const int tilecols,
        const int meshrows, const int tilerows,
        const int c_rows,
        int** A, int** B, int** D, int** expected_C) {
    int tickcount = 0;

    // init
    tb->reset = 1;
    tick(tickcount, tb, tfp);
    tb->reset = 0;
    tick(tickcount, tb, tfp);

    mat_state A_state(meshrows, tilerows, c_rows, true);
    mat_state B_state(meshcols, tilecols, meshrows * tilerows, false);
    mat_state D_state(meshcols, tilecols, c_rows, false);

    // propagate B through array
    B_state.start();
    while (true) {
        tick(tickcount, tb, tfp);
        bool done = B_state.update(B, (int**) tb->in_b, (int**) tb->in_b_valid);
        A_state.update(A, (int**) tb->in_a, (int**) tb->in_a_valid);
        D_state.update(D, (int**) tb->in_d, (int**) tb->in_d_valid);
        if (done) {
            break;
        }
    }

    // propagate A and D through array
    // TODO: collect C
    A_state.start();
    D_state.start();
    while (true) {
        tick(tickcount, tb, tfp);
        B_state.update(B, (int**) tb->in_b, (int**) tb->in_b_valid);
        bool a_done = A_state.update(A, (int**) tb->in_a, (int**) tb->in_a_valid);
        bool d_done = D_state.update(D, (int**) tb->in_d, (int**) tb->in_d_valid);
        if (a_done && d_done) {
            break;
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