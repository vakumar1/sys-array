#include "instr_utils.h"

#define BLANK std::string(" ")

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

std::string print_hex_int(unsigned int i) {
    std::ostringstream oss;
    oss << std::hex << std::setw(8) << std::setfill('0') << i;
    return std::string("0x") + oss.str();
}

std::string print_hex_char(unsigned char c) {
    std::ostringstream oss;
    oss << std::hex << std::setw(8) << std::setfill('0') << static_cast<unsigned>(c);
    return std::string("0x") + oss.str();
}

std::string print_instr(instr_t instr) {
    switch (instr.type) {
        case TERM:
            return std::string("TERM");
        case WRITE:
            return std::string("WRITE") 
                + BLANK + std::string("HEADER=") + print_hex_char(instr.inner_instr.w.header)
                + BLANK + std::string("ADDRESS=" + print_hex_char(instr.inner_instr.w.bmem_addr));
        case LOAD:
            return std::string("LOAD") 
                + BLANK + std::string("ADDRESS=") + print_hex_char(instr.inner_instr.l.b_addr);
        case COMP:
            return std::string("COMP") 
                + BLANK + std::string("A_ADDR=") + print_hex_char(instr.inner_instr.c.a_addr)
                + BLANK + std::string("D_ADDR=") + print_hex_char(instr.inner_instr.c.d_addr)
                + BLANK + std::string("C_ADDR=") + print_hex_char(instr.inner_instr.c.c_addr);
        default:
            throw std::runtime_error("Unaccepted instruction type");
    }
}