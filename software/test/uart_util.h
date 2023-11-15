#include "Vuart.h"
#include "verilated.h"
#include "verilated_vcd_c.h"

#include <vector>

#define SYMBOL_TICK_COUNT 1085

void sender_byte_bits(char data, std::vector<int>& result);
void sender_init(int& tickcount, Vuart* sender, VerilatedVcdC* tfp);
void sender_tick(Vuart* sender, int& tickcount, VerilatedVcdC* tfp,
                    char data, char data_valid, char receiver_rts);
void tick(int& tickcount, Vuart* tb, VerilatedVcdC* tfp);
