#include "instr_utils.h"

unsigned int term_instr_to_bits(term_instr_t t) {
    return TERM_CODE;
}

unsigned int write_instr_to_bits(write_instr_t w) {
    return 0 | (w.header << 10) | (w.bmem_addr << 2) | (WRITE_CODE);
} 

unsigned int load_instr_to_bits(load_instr_t l) {
    return 0 | (l.b_addr << 2) | (LOAD_CODE);
}

unsigned int comp_instr_to_bits(comp_instr_t c) {
    return 0 | (c.c_addr << 18) | (c.d_addr << 10) | (c.a_addr << 2) | (COMP_CODE);
}
