#include "virtual_device.h"

void device_log(std::string header, std::string msg) {
    std::cout << "[" + header + "] " + msg << std::endl;
}

void reading_state_t::init_reading_state() {
    this->running = false;
    this->curr_reading_byte_state = 0x0;
    this->curr_reading_byte_bit = 8;
    this->curr_reading_byte_tick = 0;
}

void virtual_device::init_device(Vuart* driver_uart, Vcore* core) {
    this->driver_uart = driver_uart;
    this->core = core;
    this->driver_uart_tickcount = 0;
    this->core_tickcount = 0;
    this->driver_uart_tfp = new VerilatedVcdC;
    this->core_tfp = new VerilatedVcdC;

    this->core->trace(this->core_tfp, 500);
    this->core_tfp->open("core.vcd");
    init(this->core_tickcount, this->core, this->core_tfp);

    this->driver_uart->trace(this->driver_uart_tfp, 99);
    this->driver_uart_tfp->open("driver_uart.vcd");
    sender_init(this->driver_uart_tickcount, this->driver_uart, this->driver_uart_tfp);

    this->read_bytes.clear();
    this->reading_state.init_reading_state();
}

void virtual_device::virtual_device_tick(char data, char data_valid) {
    // record read byte data + state
    if (!this->reading_state.running) {
        if (core->serial_out == 0) {
            this->reading_state.running = true;
        }
    }
    if (this->reading_state.running) {
        if (this->reading_state.curr_reading_byte_bit >= 0 
            && this->reading_state.curr_reading_byte_bit <= 7
            && this->reading_state.curr_reading_byte_tick % SYMBOL_TICK_COUNT == SYMBOL_TICK_COUNT / 2) {
            int bit_set = core->serial_out;
            this->reading_state.curr_reading_byte_state = (this->reading_state.curr_reading_byte_state << 1) | bit_set;
        }
        this->reading_state.curr_reading_byte_tick++;
        if (this->reading_state.curr_reading_byte_tick == SYMBOL_TICK_COUNT) {
            this->reading_state.curr_reading_byte_bit--;
            this->reading_state.curr_reading_byte_tick = 0;
        }
        if (this->reading_state.curr_reading_byte_bit == -2) {
            this->read_bytes.push_back(this->reading_state.curr_reading_byte_state);
            this->reading_state.init_reading_state();
        }
    }

    // tick driver UART + core
    sender_tick(this->driver_uart, this->driver_uart_tickcount, this->driver_uart_tfp, data, data_valid, 1);
    core_tick(this->core_tickcount, this->core, this->core_tfp, this->driver_uart->serial_out);
    core->serial_in = driver_uart->serial_out;
}

void virtual_device::virtual_device_tick() {
    this->virtual_device_tick(0x0, 0x0);
}

void virtual_device::sync_send_byte(unsigned char byte) {
    this->virtual_device_tick(byte, 0x1);
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < SYMBOL_TICK_COUNT; j++) {
            this->virtual_device_tick();
        }
    }
}

unsigned int virtual_device::get_read_bytes_count() {
    return this->read_bytes.size();
}

std::vector<unsigned char> virtual_device::get_read_bytes() {
    return this->read_bytes;
}

void virtual_device::clear_read_bytes(unsigned int size) {
    this->read_bytes.erase(this->read_bytes.begin(), this->read_bytes.begin() + size);
}

void virtual_device::thread_update(std::array<bool, 4> update_state) {
    this->sync_send_byte(UPDATE_BYTE(update_state[0], update_state[1], update_state[2], update_state[3]));
}

void virtual_device::imem_store(unsigned int imem_addr, unsigned int imem_data) {

    // send imem address and data
    this->sync_send_byte(IMEM);
    for (int i = 0; i < 4; i++) {
        unsigned char byte = (unsigned char) ((imem_addr >> (i * 8)) & (0xFF));
        this->sync_send_byte(byte);
    }
    for (int i = 0; i < 4; i++) {
        unsigned char byte = (unsigned char) ((imem_data >> (i * 8)) & (0xFF));
        this->sync_send_byte(byte);
    }
}

void virtual_device::block_store(unsigned int bmem_addr, std::array<int, MESHUNITS * MESHUNITS * TILEUNITS * TILEUNITS>& bmem_data) {
    
    // send bmem address and data
    this->sync_send_byte(BMEM);
    for (int i = 0; i < 4; i++) {
        unsigned char byte = (unsigned char) ((bmem_addr >> (i * 8)) & (0xFF));
        this->sync_send_byte(byte);
    }
    for (int i = 0; i < (MESHUNITS * MESHUNITS * TILEUNITS * TILEUNITS); i++) {
        for (int j = 0; j < 4; j++) {
            unsigned char byte = (unsigned char) ((bmem_data[i] >> (j * 8)) & (0xFF));
            this->sync_send_byte(byte);
        }
    }
}

void virtual_device::read_bmem(unsigned char& header, std::array<int, MESHUNITS * MESHUNITS * TILEUNITS * TILEUNITS>& bmem_data) {
    std::vector<unsigned char> curr_bytes;

    // wait until byte count has been sent and validate byte count
    while (this->get_read_bytes_count() < 4) {
        this->virtual_device_tick();
    }
    curr_bytes = this->get_read_bytes();
    unsigned int byte_count = 0x0;
    for (int i = 0; i < 4; i++) {
        byte_count = byte_count | (curr_bytes.at(i) << (i * 8));
    }
    device_log(std::string("READ_BMEM"), std::string("READ BYTE COUNT: ") + std::to_string(byte_count));
    if (byte_count != 1 + 4 * bmem_data.size()) {
        throw std::runtime_error("Unexpected byte count received from device - expected " + std::to_string(1 + bmem_data.size()));
    }
    this->clear_read_bytes(4);
    
    // wait until all header + data bytes are sent
    int i = 0;
    while (this->get_read_bytes_count() < byte_count) {
        this->virtual_device_tick();
        if (i % ((unsigned int) 1e6) == 0) {
            device_log(std::string("READ_BMEM"), std::to_string(i) + std::string(" CURR READ BYTE COUNT: ") + std::to_string(this->get_read_bytes_count()));
        }
        i++;
    }

    // decode header + data bytes and clear read bytes
    std::vector<unsigned char> bytes = this->get_read_bytes();
    header = bytes[0];
    for (int i = 0; i < bmem_data.size(); i++) {
        unsigned int data = 0x0;
        for (int j = 0; j < 4; j++) {
            data = data | (bytes[1 + i * 4 + j] << (j * 8));
        }
        bmem_data[i] = data;
    }
    this->clear_read_bytes(1 + 4 * bmem_data.size());

}
