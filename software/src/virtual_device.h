#include "Vuart.h"
#include "Vcore.h"
#include "Vcore_core.h"

#include "utils/uart_utils.h"
#include "utils/core_utils.h"

#include "verilated.h"
#include "verilated_vcd_c.h"

#include <iostream>
#include <vector>
#include <string>

// abstract system representation of the driver UART device - 
// allows synchronous byte sends and asynchronous byte reads in the background (where the caller drives ticks)
typedef struct {
    bool running;
    unsigned char curr_reading_byte_state;
    int curr_reading_byte_bit;
    unsigned int curr_reading_byte_tick;

    void init_reading_state();
} reading_state_t;


class virtual_device {
private:
    Vuart* driver_uart;
    Vcore* core;
    int driver_uart_tickcount;
    int core_tickcount;
    VerilatedVcdC* driver_uart_tfp;
    VerilatedVcdC* core_tfp;
    std::vector<unsigned char> read_bytes;
    reading_state_t reading_state;

    void virtual_device_tick(char data, char data_valid);

public:
    void init_device(Vuart* driver_uart, Vcore* core);
    void sync_send_byte(unsigned char byte);
    void virtual_device_tick();
    unsigned int get_read_bytes_count();
    std::vector<unsigned char> get_read_bytes();
    void clear_read_bytes(unsigned int size);
    void thread_update(std::array<bool, 4> update_state);
    void imem_store(unsigned int imem_addr, unsigned int imem_data);
    void block_store(unsigned int bmem_addr, std::array<int, MESHUNITS * MESHUNITS * TILEUNITS * TILEUNITS>& bmem_data);
    void read_bmem(unsigned char& header, std::array<int, MESHUNITS * MESHUNITS * TILEUNITS * TILEUNITS>& bmem_data);
};
