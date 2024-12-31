//
// INSTRUCTION DATA TYPES
//

#include <string>
#include <iomanip>
#include <sstream>

#define TERM_CODE 0b00
#define WRITE_CODE 0b01
#define LOAD_CODE 0b10
#define COMP_CODE 0b11

enum instr_type {
    TERM,
    WRITE,
    LOAD,
    COMP
};

// TERM instr.
typedef struct {} 
term_instr_t;

unsigned int term_instr_to_bits(term_instr_t t);

// WRITE instr.
typedef struct {
    unsigned char header;
    unsigned char bmem_addr;
} write_instr_t;

unsigned int write_instr_to_bits(write_instr_t w);

// LOAD instr.
typedef struct {
    unsigned char b_addr;
} load_instr_t;

unsigned int load_instr_to_bits(load_instr_t l);

// COMP instr.
typedef struct {
    unsigned char a_addr;
    unsigned char d_addr;
    unsigned char c_addr;
} comp_instr_t;

unsigned int comp_instr_to_bits(comp_instr_t c);

// instr. wrapper
typedef struct {
    instr_type type;
    union {
        term_instr_t t;
        write_instr_t w;
        load_instr_t l;
        comp_instr_t c;
    } inner_instr;
} instr_t;

std::string print_hex_int(unsigned int i);
std::string print_hex_char(unsigned char c);
std::string print_instr(instr_t instr);
