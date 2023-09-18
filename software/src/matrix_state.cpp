#include "matrix_state.h"

#include "verilated.h"

#include <stdlib.h>
#include <vector>

// FEEDING MATRIX STATE

// init state and kickoff feeding
void feeding_mat_state::start() {
    if (this->dummy) {
        for (int i = 0; i < mesh_size; i++) {
            this->mesh_unit_state[i] = DONE;
            this->mesh_unit_row[i] = 0;
        }
        return;
    }
    int start_row = this->ordered_rows ? 0 : mat_rows - 1;
    for (int i = 0; i < mesh_size; i++) {
        this->mesh_unit_state[i] = WAITING;
        this->mesh_unit_row[i] = start_row;
    }
    this->mesh_unit_state[0] = PROCESSING;
}

int feeding_mat_state::current_row(int mesh_unit) {
    return this->mesh_unit_row[mesh_unit];
}

// READING MATRIX STATE

// init state and kickoff feeding
void reading_mat_state::start() {
    if (this->dummy) {
        for (int i = 0; i < mesh_size; i++) {
            this->mesh_unit_state[i] = DONE;
            this->mesh_unit_row[i] = 0;
        }
        this->valid_state = true;
        return;
    }
    int start_row = this->ordered_rows ? 0 : mat_rows - 1;
    for (int i = 0; i < mesh_size; i++) {
        this->mesh_unit_state[i] = WAITING;
        this->mesh_unit_row[i] = start_row;
    }
    this->valid_state = true;
}

bool reading_mat_state::valid() {
    if (this->dummy) {
        return true;
    }
    return this->valid_state;
}

