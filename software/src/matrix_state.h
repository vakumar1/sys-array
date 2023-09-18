#include "verilated.h"

#include <stdlib.h>
#include <vector>

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
    bool dummy;

public:
    feeding_mat_state(int mesh_size, int tile_size) {
        this->mesh_size = mesh_size;
        this->tile_size = tile_size;
        this->mat_rows = 1;
        this->mesh_unit_state.resize(mesh_size);
        this->mesh_unit_row.resize(mesh_size);
        this->ordered_rows = true;
        this->dummy = true;
    }

    feeding_mat_state(int mesh_size, int tile_size, int mat_rows, bool ordered_rows) {
        this->mesh_size = mesh_size;
        this->tile_size = tile_size;
        this->mat_rows = mat_rows;
        this->mesh_unit_state.resize(mesh_size);
        this->mesh_unit_row.resize(mesh_size);
        this->ordered_rows = ordered_rows;
        this->dummy = false;
    }

    void start();
    int current_row(int mesh_unit);
    
    // update the internal feed/read state by one tick
    template <size_t _mat_cols, size_t _mesh_size, size_t _tile_size>
    bool update(size_t mat_rows, std::vector<std::vector<int>>& in_mat, IData out_mat[_mesh_size][_tile_size], CData out_valid[_mesh_size][_tile_size]) {
        mat_stage next_mesh_unit_state[mesh_size];
        for (int mesh_unit = 0; mesh_unit < mesh_size; mesh_unit++) {
            switch (this->mesh_unit_state[mesh_unit]) {
            case WAITING:
                // feed in invalid signal to the array for each tile unit
                for (int tile_unit = 0; tile_unit < tile_size; tile_unit++) {
                    out_valid[mesh_unit][tile_unit] = 0x00;
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
                    out_valid[mesh_unit][tile_unit] = 0xFF;
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
                    out_valid[mesh_unit][tile_unit] = 0x00;
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
    bool dummy;
public:
    reading_mat_state(int mesh_size, int tile_size) {
        this->mesh_size = mesh_size;
        this->tile_size = tile_size;
        this->mat_rows = 1;
        this->mesh_unit_state.resize(mesh_size);
        this->mesh_unit_row.resize(mesh_size);
        this->ordered_rows = true;
        this->valid_state = false;
        this->dummy = true;
    }
    reading_mat_state(int mesh_size, int tile_size, int mat_rows, bool ordered_rows) {
        this->mesh_size = mesh_size;
        this->tile_size = tile_size;
        this->mat_rows = mat_rows;
        this->mesh_unit_state.resize(mesh_size);
        this->mesh_unit_row.resize(mesh_size);
        this->ordered_rows = ordered_rows;
        this->valid_state = false;
        this->dummy = false;
    }

    void start();
    bool valid();
    // update the internal read state by one tick
    template <size_t _mat_cols, size_t _mesh_size, size_t _tile_size>
    bool update(size_t mat_rows, IData in_mat[_mesh_size][_tile_size], CData in_valid[_mesh_size][_tile_size], std::vector<std::vector<int>>& out_mat) {
        mat_stage next_mesh_unit_state[mesh_size];
        bool invalid_mesh_unit[mesh_size];
        for (int mesh_unit = 0; mesh_unit < mesh_size; mesh_unit++) {
            invalid_mesh_unit[mesh_unit] = false;
        }
        for (int mesh_unit = 0; mesh_unit < mesh_size; mesh_unit++) {
            // record valid signals and the output values (if valid)
            int valid_count = 0;
            for (int tile_unit = 0; tile_unit < tile_size; tile_unit++) {
                if (in_valid[mesh_unit][tile_unit] == 0xFF) {
                    out_mat[this->mesh_unit_row[mesh_unit]][mesh_unit * tile_size + tile_unit] = in_mat[mesh_unit][tile_unit];
                    valid_count++;
                } else if (in_valid[mesh_unit][tile_unit] != 0x00) {
                    invalid_mesh_unit[mesh_unit] = true;
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

