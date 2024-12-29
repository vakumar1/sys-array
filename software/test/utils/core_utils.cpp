#include "test_utils.h"
#include "core_utils.h"
#include "uart_utils.h"
#include "instr_utils.h"

void tick(int& tickcount, Vcore* tb, VerilatedVcdC* tfp, int serial_in) {
    tb->serial_in = serial_in;
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

void init(int& tickcount, Vcore* tb, VerilatedVcdC* tfp) {
    tb->reset = 1;
    tick(tickcount, tb, tfp, 1);
    tb->reset = 0;

    // needed to overwrite default of serial=0 (which indicates TX start)
    tb->serial_in = 1;
    tick(tickcount, tb, tfp, 1);
}

//
// DRIVER WRITE UTILS
//

void core_tick(int& tickcount, Vcore* tb, VerilatedVcdC* tfp, int serial_in) {
    tick(tickcount, tb, tfp, serial_in);
}

void wait_on_final_bit(int& driver_tickcount, Vuart* driver_uart, VerilatedVcdC* driver_tfp, 
                        int& core_tickcount, Vcore* core, VerilatedVcdC* tfp) {

    char signal_msg[100];

    // send final bit to close connection
    for (int i = 0; i < SYMBOL_TICK_COUNT; i++) {
        sprintf(signal_msg, "[bit %d][iter %d] driver_uart->serial_out", 9, i);
        signal_err(signal_msg, 0, driver_uart->serial_out);
        sender_tick(driver_uart, driver_tickcount, driver_tfp, 0x0, 0, 1);
        tick(core_tickcount, core, tfp, 1);
        core->serial_in = driver_uart->serial_out;
    }
}

void send_byte(int& driver_tickcount, Vuart* driver_uart, VerilatedVcdC* driver_tfp, 
                int& core_tickcount, Vcore* core, VerilatedVcdC* tfp, unsigned char byte) {
    
    // wait a random number of cycles to catch edge errors
    for (int i = 0; i < SYMBOL_TICK_COUNT + (std::rand() % 5); i++) {
        sender_tick(driver_uart, driver_tickcount, driver_tfp, 0x0, 0, 1);
        tick(core_tickcount, core, tfp, 1);
        core->serial_in = driver_uart->serial_out;
    }

    // init sender with data
    sender_tick(driver_uart, driver_tickcount, driver_tfp, byte, 1, 1);
    tick(core_tickcount, core, tfp, driver_uart->serial_out);

    char signal_msg[100];

    // send initial bit and initialize receiver to READING state
    for (int i = 0; i < SYMBOL_TICK_COUNT; i++) {
        sprintf(signal_msg, "[bit %d][iter %d] driver_uart->serial_out", 0, i);
        signal_err(signal_msg, 0, driver_uart->serial_out);

        sender_tick(driver_uart, driver_tickcount, driver_tfp, 0x0, 0, 1);
        tick(core_tickcount, core, tfp, driver_uart->serial_out);
        core->serial_in = driver_uart->serial_out;
    }

    // send bits 0-7
    for (int bit_pos = 7; bit_pos >= 0; bit_pos--) {
        int expected_set = (byte >> bit_pos) & 0x1;
        for (int i = 0; i < SYMBOL_TICK_COUNT; i++) {
            sprintf(signal_msg, "[bit %d][iter %d] driver_uart->serial_out", bit_pos + 1, i);
            signal_err(signal_msg, expected_set, driver_uart->serial_out);

            sender_tick(driver_uart, driver_tickcount, driver_tfp, 0x0, 0, 1);
            tick(core_tickcount, core, tfp, driver_uart->serial_out);
            core->serial_in = driver_uart->serial_out;
        }
    }

    // ** this is where we would send final bit to close connection
    //    we may opt out because 
    //    (i) the UART RX immediately outputs valid data for the core to read at the START of this byte --> we immediately need to prepare
    //        for the thread to start running
    //    (ii) the UART RX does not actually verify the serial in data is 0 - so we can ignore setting serial in correctly
    //    
    // ** since we need the UART RX to be ready to receive future bytes we spin for at least SYMBOL_TICK_COUNT cycles
    //    at the beginning of send_byte 
    //
    // ** the driver UART TX needs to complete its cycles --> we allow the driver UART to spin on its own

    // reset core RX serial in
    core->serial_in = 1;
}

int thread_update(int& driver_tickcount, Vuart* driver_uart, VerilatedVcdC* driver_tfp, int& core_tickcount, Vcore* core, VerilatedVcdC* tfp, 
                std::array<bool, 4> update_state) {

    // validate update and send
    char update_msg[100];
    sprintf(update_msg, "Invalid update t0=%d t1=%d t2=%d", update_state[0], update_state[1], update_state[2]);
    condition_err(update_msg, update_state[0] && update_state[1] && update_state[2]);
    send_byte(driver_tickcount, driver_uart, driver_tfp, core_tickcount, core, tfp, 
        UPDATE_BYTE(update_state[0], update_state[1], update_state[2], update_state[3]));
    return SUCCESS;
}

int imem_store(int& driver_tickcount, Vuart* driver_uart, VerilatedVcdC* driver_tfp, int& core_tickcount, Vcore* core, VerilatedVcdC* tfp, 
                int write_imem, unsigned int imem_addr, unsigned int imem_data) {

    // send imem address and data
    send_byte(driver_tickcount, driver_uart, driver_tfp, core_tickcount, core, tfp, IMEM);
    for (int i = 0; i < 4; i++) {
        unsigned char byte = (unsigned char) ((imem_addr >> (i * 8)) & (0xFF));
        send_byte(driver_tickcount, driver_uart, driver_tfp, core_tickcount, core, tfp, byte);
    }
    for (int i = 0; i < 4; i++) {
        unsigned char byte = (unsigned char) ((imem_data >> (i * 8)) & (0xFF));
        send_byte(driver_tickcount, driver_uart, driver_tfp, core_tickcount, core, tfp, byte);
    }

    // check that imem was correctly stored
    wait_on_final_bit(driver_tickcount, driver_uart, driver_tfp, core_tickcount, core, tfp);
    Vcore_imem__A100_B20* imem = core->core->_imem0;
    // write_imem == 0 ? core->core->_imem0
    //                             : write_imem == 1 ? core->core->_imem1
    //                             : core->core->_imem2;
    unsigned int actual_imem_data = imem->instr_mem[(imem_addr >> 2) & (IMEM_ADDRSIZE - 1)];
    data_err("IMEM[" + std::to_string((imem_addr >> 2) & (IMEM_ADDRSIZE - 1)) + "]", imem_data, actual_imem_data);
    return SUCCESS;
}

int block_store(int& driver_tickcount, Vuart* driver_uart, VerilatedVcdC* driver_tfp, int& core_tickcount, Vcore* core, VerilatedVcdC* tfp, 
                unsigned int bmem_addr, std::array<int, MESHUNITS * MESHUNITS * TILEUNITS * TILEUNITS>& bmem_data) {
    
    // send bmem address and data
    send_byte(driver_tickcount, driver_uart, driver_tfp, core_tickcount, core, tfp, BMEM);
    for (int i = 0; i < 4; i++) {
        unsigned char byte = (unsigned char) ((bmem_addr >> (i * 8)) & (0xFF));
        send_byte(driver_tickcount, driver_uart, driver_tfp, core_tickcount, core, tfp, byte);
    }
    for (int i = 0; i < (MESHUNITS * MESHUNITS * TILEUNITS * TILEUNITS); i++) {
        for (int j = 0; j < 4; j++) {
            unsigned char byte = (unsigned char) ((bmem_data[i] >> (j * 8)) & (0xFF));
            send_byte(driver_tickcount, driver_uart, driver_tfp, core_tickcount, core, tfp, byte);
        }
    }

    // check that bmem was correctly stored
    wait_on_final_bit(driver_tickcount, driver_uart, driver_tfp, core_tickcount, core, tfp);
    unsigned int mask_bmem_addr = ((bmem_addr >> 2) << 2) & (BMEM_ADDRSIZE - 1);
    for (int i = 0; i < (MESHUNITS * MESHUNITS * TILEUNITS * TILEUNITS); i++) {
        unsigned int actual_bmem_data = core->core->_blockmem->block_mem[mask_bmem_addr + i];
        data_err("BMEM[" + std::to_string(mask_bmem_addr) + "+" + std::to_string(i) + "]", bmem_data[i], actual_bmem_data);
    }
    return SUCCESS;

}

//
// DRIVER READ UTILS
//

unsigned char read_byte(int& driver_tickcount, Vuart* driver_uart, VerilatedVcdC* driver_tfp,
                        int& core_tickcount, Vcore* core, VerilatedVcdC* tfp) {


    // wait until UART starts writing to serial_out
    for (int cycle_count = 0; ; cycle_count++) {
        if (core->serial_out == 0) {
            break;
        }
        tick(core_tickcount, core, tfp, 1);
        condition_err("Timed out waiting to read from UART", cycle_count >= 100000);
    }

    char signal_msg[100];
    unsigned char actual_byte = 0x0;
    int bit_set = 0;

    // receive initial bit
    for (int i = 0; i < SYMBOL_TICK_COUNT; i++) {
        sprintf(signal_msg, "[bit %d][iter %d] core->serial_out", 0, i);
        signal_err(signal_msg, 0, core->serial_out);
        tick(core_tickcount, core, tfp, 1);
    }

    // receive bits 0-7
    for (int bit_pos = 7; bit_pos >= 0; bit_pos--) {
        for (int i = 0; i < SYMBOL_TICK_COUNT; i++) {
            if (i == 0) {
                bit_set = core->serial_out;
                actual_byte = (actual_byte << 1) | bit_set;
            }
            tick(core_tickcount, core, tfp, 1);
        }
    }

    // receive final bit
    for (int i = 0; i < SYMBOL_TICK_COUNT; i++) {
        sprintf(signal_msg, "[bit %d][iter %d] core->serial_out", 9, i);
        signal_err(signal_msg, 0, core->serial_out);
        tick(core_tickcount, core, tfp, 1);
    }

    return actual_byte;
}

int read_bmem(int& driver_tickcount, Vuart* driver_uart, VerilatedVcdC* driver_tfp,
                int& core_tickcount, Vcore* core, VerilatedVcdC* tfp, unsigned int bmem_addr,
                unsigned char header, std::array<int, MESHUNITS * MESHUNITS * TILEUNITS * TILEUNITS>& bmem_data) {
    
    char signal_msg[100];
    unsigned char expected_byte;
    unsigned char actual_byte;

    // read byte count
    unsigned int byte_count = 1 + 4 * MESHUNITS * MESHUNITS * TILEUNITS * TILEUNITS;
    for (int i = 0; i < 4; i++) {
        expected_byte = (unsigned char) ((byte_count >> (i * 8)) & 0xFF);
        actual_byte = read_byte(driver_tickcount, driver_uart, driver_tfp, core_tickcount, core, tfp);

        sprintf(signal_msg, "write - byte_count[%d]", i);
        data_err(signal_msg, expected_byte, actual_byte);
    }

    // read header
    expected_byte = header;
    actual_byte = read_byte(driver_tickcount, driver_uart, driver_tfp, core_tickcount, core, tfp);
    sprintf(signal_msg, "write - header");
    data_err(signal_msg, expected_byte, actual_byte);

    // read bmem data
    for (int i = 0; i < bmem_data.size(); i++) {
        for (int j = 0; j < 4; j++) {
            expected_byte = (unsigned char) ((bmem_data[i] >> (j * 8)) & 0xFF);
            actual_byte = read_byte(driver_tickcount, driver_uart, driver_tfp, core_tickcount, core, tfp);
            sprintf(signal_msg, "write - bmem[%d][%d]", i, j);
            data_err(signal_msg, expected_byte, actual_byte);
        }
    }
    return SUCCESS;
}
