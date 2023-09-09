#include <stdio.h>
#include <stdlib.h>
#include "Vsys_array.h"
#include "verilated.h"
#include "verilated_vcd_c.h"

#define SUCCESS 0
#define SIM_ERROR 1
#define INTERNAL_ERROR 2

#ifndef BITWIDTH
#define BITWIDTH 32
#endif

#ifndef MESHCOLS
#define MESHCOLS 4
#endif

#ifndef TILECOLS
#define TILECOLS 1
#endif

#ifndef MESHROWS
#define MESHROWS 4
#endif

#ifndef TILEROWS 
#define TILEROWS 1
#endif

enum mat_stage {
    WAITING = 0,
    PROCESSING = 1,
    DONE = 2
};

class feeding_mat_state {
private:
    int mesh_size;
    int tile_size;
    int mat_rows;
    std::vector<mat_stage> mesh_unit_state;
    std::vector<int> mesh_unit_row;
    bool ordered_rows;
public:
    feeding_mat_state(int mesh_size, int tile_size, int mat_rows, bool ordered_rows) {
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
            this->mesh_unit_state[i] = WAITING;
            this->mesh_unit_row[i] = start_row;
        }
        this->mesh_unit_state[0] = PROCESSING;
    }

    int current_row(int mesh_unit) {
        return this->mesh_unit_row[mesh_unit];
    }

    // update the internal feed/read state by one tick
    template <size_t mat_rows, size_t mat_cols, size_t mesh_size, size_t tile_size>
    bool update(int in_mat[mat_rows][mat_cols], IData out_mat[mesh_size][tile_size], CData out_valid[mesh_size][tile_size]) {
        mat_stage next_mesh_unit_state[mesh_size];
        for (int mesh_unit = 0; mesh_unit < mesh_size; mesh_unit++) {
            switch (this->mesh_unit_state[mesh_unit]) {
            case WAITING:
                // feed in invalid signal to the array for each tile unit
                for (int tile_unit = 0; tile_unit < tile_size; tile_unit++) {
                    out_valid[mesh_unit][tile_unit] = 0;
                }

                // update feeding to true if the previous mesh unit is feeding/done
                next_mesh_unit_state[mesh_unit] = mesh_unit > 0 && this->mesh_unit_state[mesh_unit - 1] != WAITING
                                                    ? PROCESSING
                                                    : WAITING;
                break;

            case PROCESSING:
                // feed in the input matrix (+ valid signal) to the array for each tile row in given mesh_row
                for (int tile_unit = 0; tile_unit < tile_size; tile_unit++) {
                    out_mat[mesh_unit][tile_unit] = in_mat[this->mesh_unit_row[mesh_unit]][mesh_unit * tile_size + tile_unit];
                    out_valid[mesh_unit][tile_unit] = 1;
                }

                // incr input matrix row (and end feeding if complete)
                if (this->ordered_rows) {
                    this->mesh_unit_row[mesh_unit]++;
                    next_mesh_unit_state[mesh_unit] = this->mesh_unit_row[mesh_unit] >= mat_rows
                                                        ? DONE
                                                        : PROCESSING;
                } else {
                    this->mesh_unit_row[mesh_unit]--;
                    next_mesh_unit_state[mesh_unit] = this->mesh_unit_row[mesh_unit] < 0
                                                        ? DONE
                                                        : PROCESSING;
                }
                break;

            case DONE:
                // feed in invalid signal to the array for each tile row
                for (int tile_unit = 0; tile_unit < tile_size; tile_unit++) {
                    out_valid[mesh_unit][tile_unit] = 0;
                }
                next_mesh_unit_state[mesh_unit] = DONE;
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

class reading_mat_state {
private:
    int mesh_size;
    int tile_size;
    int mat_rows;
    std::vector<mat_stage> mesh_unit_state;
    std::vector<int> mesh_unit_row;
    bool ordered_rows;
    bool valid_state;
public:
    reading_mat_state(int mesh_size, int tile_size, int mat_rows, bool ordered_rows) {
        this->mesh_size = mesh_size;
        this->tile_size = tile_size;
        this->mat_rows = mat_rows;
        this->mesh_unit_state.resize(mesh_size);
        this->mesh_unit_row.resize(mesh_size);
        this->ordered_rows = ordered_rows;
        this->valid_state = false;
    }

    // init state and kickoff feeding
    void start() {
        int start_row = this->ordered_rows ? 0 : mat_rows - 1;
        for (int i = 0; i < mesh_size; i++) {
            this->mesh_unit_state[i] = WAITING;
            this->mesh_unit_row[i] = start_row;
        }
        this->valid_state = true;
    }

    bool valid() {
        return this->valid_state;
    }

    // update the internal read state by one tick
    template <size_t mat_rows, size_t mat_cols, size_t mesh_size, size_t tile_size>
    bool update(IData in_mat[mesh_size][tile_size], CData in_valid[mesh_size][tile_size], int out_mat[mat_rows][mat_cols]) {
        mat_stage next_mesh_unit_state[mesh_size];
        bool invalid_mesh_unit[mesh_size];
        for (int mesh_unit = 0; mesh_unit < mesh_size; mesh_unit++) {
            invalid_mesh_unit[mesh_unit] = false;
        }
        for (int mesh_unit = 0; mesh_unit < mesh_size; mesh_unit++) {
            // record valid signals and the output values (if valid)
            int valid_count = 0;
            for (int tile_unit = 0; tile_unit < tile_size; tile_unit++) {
                if (in_valid[mesh_unit][tile_unit]) {
                    out_mat[this->mesh_unit_row[mesh_unit]][mesh_unit * tile_size + tile_unit] = in_mat[mesh_unit][tile_unit];
                    valid_count++;
                }
            }
            if (valid_count != 0 && valid_count != tile_size) {
                invalid_mesh_unit[mesh_unit] = true;
                continue;
            }

            if (valid_count == tile_size) {
                switch (this->mesh_unit_state[mesh_unit]) {
                case WAITING:
                case PROCESSING:
                    // incr output matrix row (and end reading if complete)
                    if (this->ordered_rows) {
                        this->mesh_unit_row[mesh_unit]++;
                        next_mesh_unit_state[mesh_unit] = this->mesh_unit_row[mesh_unit] >= mat_rows
                                                            ? DONE
                                                            : PROCESSING;
                    } else {
                        this->mesh_unit_row[mesh_unit]--;
                        next_mesh_unit_state[mesh_unit] = this->mesh_unit_row[mesh_unit] < 0
                                                            ? DONE
                                                            : PROCESSING;
                    }
                    break;

                case DONE:
                    invalid_mesh_unit[mesh_unit] = true;
                    next_mesh_unit_state[mesh_unit] = DONE;
                    break;
                }
            } else {
                switch (this->mesh_unit_state[mesh_unit]) {
                case WAITING:
                case DONE:
                    next_mesh_unit_state[mesh_unit] = this->mesh_unit_state[mesh_unit];
                    break;

                case PROCESSING:
                    next_mesh_unit_state[mesh_unit] = DONE;
                    break;
                }
            }
        }

        // detect if the matrix reader has entered an invalid state
        for (int mesh_unit = 0; mesh_unit < mesh_size; mesh_unit++) {
            this->valid_state = this->valid_state && !invalid_mesh_unit[mesh_unit];
            this->valid_state = this->valid_state && 
                                    (mesh_unit == 0 || this->mesh_unit_state[mesh_unit] <= this->mesh_unit_state[mesh_unit - 1]);
        }

        for (int mesh_unit = 0; mesh_unit < mesh_size; mesh_unit++) {
            this->mesh_unit_state[mesh_unit] = next_mesh_unit_state[mesh_unit];
        }

        // reading is complete once the last mesh unit has finished
        return this->mesh_unit_state[mesh_size - 1] == DONE;
    }
};


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


template <size_t c_rows>
int basic_matmul(int& tickcount, Vsys_array* tb, VerilatedVcdC* tfp, 
        int A[c_rows][MESHROWS * TILEROWS], int B[MESHROWS * TILEROWS][MESHCOLS * TILECOLS], 
        int D[c_rows][MESHCOLS * TILECOLS], int expected_C[c_rows][MESHCOLS * TILECOLS]) {

    // init
    tb->reset = 1;
    tick(tickcount, tb, tfp);
    tb->reset = 0;
    tick(tickcount, tb, tfp);

    feeding_mat_state A_state(MESHROWS, TILEROWS, c_rows, true);
    feeding_mat_state B_state(MESHCOLS, TILECOLS, MESHROWS * TILEROWS, false);
    reading_mat_state C_state(MESHCOLS, TILECOLS, c_rows, true);
    feeding_mat_state D_state(MESHCOLS, TILECOLS, c_rows, false);

    // propagate B through array
    unsigned char propagate = 0;
    B_state.start();
    while (true) {
        tick(tickcount, tb, tfp);
        for (int i = 0; i < MESHCOLS; i++) {
            for (int j = 0; j < TILECOLS; j++) {
                tb->in_propagate[i][j] = propagate;
                tb->in_b_shelf_life[i][j] = B_state.current_row(i) + 1;
            }
        }
        bool done = B_state.update<MESHROWS * TILEROWS, MESHCOLS * TILECOLS, MESHCOLS, TILECOLS>(B, tb->in_b, tb->in_b_valid);
        A_state.update<c_rows, MESHROWS * TILEROWS, MESHROWS, TILEROWS>(A, tb->in_a, tb->in_a_valid);
        D_state.update<c_rows, MESHCOLS * TILECOLS, MESHCOLS, TILECOLS>(D, tb->in_d, tb->in_d_valid);

        if (done) {
            break;
        }
    }

    // propagate A and D through array
    // loop until A and D are done feeding and C is done reading, or C becomes invalid
    propagate = 1;
    A_state.start();
    D_state.start();
    C_state.start();
    int C[c_rows][MESHCOLS * TILECOLS];
    bool c_valid = true;
    int waiting_iters = 0;
    while (true) {
        tick(tickcount, tb, tfp);
        B_state.update<MESHROWS * TILEROWS, MESHCOLS * TILECOLS, MESHCOLS, TILECOLS>(B, tb->in_b, tb->in_b_valid);
        bool a_done = A_state.update<c_rows, MESHROWS * TILEROWS, MESHROWS, TILEROWS>(A, tb->in_a, tb->in_a_valid);
        bool d_done = D_state.update<c_rows, MESHCOLS * TILECOLS, MESHCOLS, TILECOLS>(D, tb->in_d, tb->in_d_valid);
        bool c_done = C_state.update<c_rows, MESHCOLS * TILECOLS, MESHCOLS, TILECOLS>(tb->out_c, tb->out_c_valid, C);
        c_valid = C_state.valid();
        for (int i = 0; i < MESHCOLS; i++) {
            for (int j = 0; j < TILECOLS; j++) {
                tb->in_propagate[i][j] = propagate;
            }
        }
        if ((a_done && d_done)) {
            waiting_iters++;
            if (c_done) {
                break;
            }
        }
        if (!c_valid || waiting_iters >= 10) {
            return SIM_ERROR;
        }
    }

    for (int i = 0; i < c_rows; i++) {
        for (int j = 0; j < MESHCOLS * TILECOLS; j++) {
            if (expected_C[i][j] != C[i][j]) {
                return SIM_ERROR;
            }
        }
    }
    return SUCCESS;
}

int test0_identity_matmul(int& tickcount, Vsys_array* tb, VerilatedVcdC* tfp) {
    const int c_rows = MESHROWS * TILEROWS;
    int A[c_rows][MESHROWS * TILEROWS];
    for (int i = 0; i < c_rows; i++) {
        for (int j = 0; j < c_rows; j++) {
            A[i][j] = i == j ? 1 : 0;
        }
    }

    int B[MESHROWS * TILEROWS][MESHCOLS * TILECOLS];
    for (int i = 0; i < MESHROWS * TILEROWS; i++) {
        for (int j = 0; j < MESHCOLS * TILECOLS; j++) {
            B[i][j] = 1;
        }
    }
    
    int D[c_rows][MESHCOLS * TILECOLS];
    int expected_C[c_rows][MESHCOLS * TILECOLS];
    for (int i = 0; i < c_rows; i++) {
        for (int j = 0; j < MESHCOLS * TILECOLS; j++) {
            D[i][j] = 0;
            expected_C[i][j] = 1;
        }
    }

    return basic_matmul<c_rows>(tickcount, tb, tfp, A, B, D, expected_C);
}

int test1_identity_matmul(int& tickcount, Vsys_array* tb, VerilatedVcdC* tfp) {
    const int c_rows = MESHROWS * TILEROWS;
    int A[c_rows][MESHROWS * TILEROWS];
    for (int i = 0; i < c_rows; i++) {
        for (int j = 0; j < c_rows; j++) {
            A[i][j] = i == j ? 1 : 0;
        }
    }

    int B[MESHROWS * TILEROWS][MESHCOLS * TILECOLS];
    for (int i = 0; i < MESHROWS * TILEROWS; i++) {
        for (int j = 0; j < MESHCOLS * TILECOLS; j++) {
            B[i][j] = i + j;
        }
    }
    
    int D[c_rows][MESHCOLS * TILECOLS];
    int expected_C[c_rows][MESHCOLS * TILECOLS];
    for (int i = 0; i < c_rows; i++) {
        for (int j = 0; j < MESHCOLS * TILECOLS; j++) {
            D[i][j] = 1;
            expected_C[i][j] = i + j + 1;
        }
    }

    return basic_matmul<c_rows>(tickcount, tb, tfp, A, B, D, expected_C);
}

int main(int argc, char** argv) {

    Verilated::commandArgs(argc, argv);
    Vsys_array* tb = new Vsys_array;
    Verilated::traceEverOn(true);
    
    int tickcount = 0;
    VerilatedVcdC* tfp = new VerilatedVcdC;
    tb->trace(tfp, 99);
    tfp->open("sys_array.vcd");
    
    int res = test0_identity_matmul(tickcount, tb, tfp);
    printf("Test 0 (Identity matmul) %s\n", ((res == SUCCESS) ? "passed" : "failed"));

    res = test1_identity_matmul(tickcount, tb, tfp);
    printf("Test 1 (Identity matmul) %s\n", ((res == SUCCESS) ? "passed" : "failed"));
    
    tfp->close();

    return 0;
}