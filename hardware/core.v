module core
    #(
        parameter BITWIDTH, IMEM_ADDRSIZE, BMEM_ADDRSIZE, MESHUNITS, TILEUNITS,
                    BAUD_RATE=115_200, CLOCK_FREQ=125_000_000, BUFFER_SIZE=256
    )
    (
        input clock,
        input reset,

        input serial_in,
        output serial_out
    );

    // loader state
    parameter
        LOADER_START,
        LOADER_IMEM_ADDR,
        LOADER_IMEM_DATA,
        LOADER_BMEM_ADDR,
        LOADER_BMEM_DATA;

    // control signal values
    parameter
        INVALID = 2'b00,
        IMEM = 2'b01,
        BMEM = 2'b10,
        UPDATE = 2'b11;
        

    reg [2:0] loader_state;
    reg [BITWIDTH-1:0] loader_byte_ctr;
    reg [BITWIDTH-1:0] loader_addr_buffer;
    reg [BITWIDTH-1:0] loader_imem_data_buffer;
    reg [BITWIDTH-1:0] loader_bmem_data_buffer [(MESHUNITS * MESHUNITS * TILEUNITS * TILEUNITS) - 1:0];
    reg thread0_running;
    reg thread1_running;
    reg thread2_running;
    always @(posedge clock) begin
        // default invalidate all loader write signals
        write_valid_bmem <= 0;
        write_valid_imem0 <= 0;
        write_valid_imem1 <= 0;
        write_valid_imem2 <= 0;

        if (reset) begin
            // -> LOADER START
            loader_state <= LOADER_START;
        end
        else begin
            if (read_data_valid) begin
                case (state)
                    LOADER_START: begin
                        case (read_data[7:6])
                            INVALID: begin
                                // TODO: echo back invalid signal to UART to confirm live
                                // -> LOADER START
                                loader_state <= LOADER_START;
                            end
                            IMEM: begin
                                // -> IMEM ADDR
                                loader_state <= LOADER_IMEM_ADDR;
                                loader_byte_ctr <= 0;
                            end
                            BMEM: begin
                                // -> BMEM ADDR
                                loader_state <= LOADER_BMEM_ADDR;
                                loader_byte_ctr <= 0;
                            end
                            UPDATE: begin
                                // update running threads
                                // -> LOADER START 
                                loader_state <= LOADER_START;
                                thread0_running <= read_data[0];
                                thread1_running <= read_data[1];
                                thread2_running <= read_data[2] & ~read_data[0] & ~read_data[1];
                            end
                        endcase
                    end
                    LOADER_IMEM_ADDR: begin
                        loader_addr_buffer[8 * (load_byte_ctr + 1) - 1:8 * load_byte_ctr] <= read_data;
                        if (load_byte_ctr == (BITWIDTH >> 2) - 1) begin
                            // -> IMEM DATA
                            load_state <= LOADER_IMEM_DATA;
                            load_byte_ctr <= 0;
                        end
                        else begin
                            load_byte_ctr <= load_byte_ctr + 1;
                        end
                    end
                    LOADER_IMEM_DATA: begin
                        loader_imem_data_buffer[8 * (load_byte_ctr + 1) - 1:8 * load_byte_ctr] <= read_data;
                        if (load_byte_ctr == (BITWIDTH >> 2) - 1) begin
                            // write imem data
                            if (~thread0_running) begin
                                write_valid_imem0 <= 1;
                            end
                            else if (~thread1_running) begin
                                write_valid_imem1 <= 1;
                            end
                            else if (~thread2_running) begin
                                write_valid_imem2 <= 1;
                            end

                            // -> START
                            load_state <= LOADER_START;
                        end
                        else begin
                            load_byte_ctr <= load_byte_ctr + 1;
                        end
                    end
                    LOADER_BMEM_ADDR: begin
                        loader_addr_buffer[8 * (load_byte_ctr + 1) - 1:8 * load_byte_ctr] <= read_data;
                        if (load_byte_ctr == (BITWIDTH >> 2) - 1) begin
                            // -> BMEM DATA
                            load_state <= LOADER_BMEM_DATA;
                            load_byte_ctr <= 0;
                        end
                        else begin
                            load_byte_ctr <= load_byte_ctr + 1;
                        end
                    end
                    LOADER_BMEM_DATA: begin
                        loader_bmem_data_buffer[] <= read_data
                        if (load_byte_ctr == (BITWIDTH >> 2) * (MESHUNITS * MESHUNITS * TILEUNITS * TILEUNITS) - 1) begin
                            // write bmem data
                            write_valid_bmem <= 1;

                            // -> START
                            load_state <= LOADER_START;
                        end
                        else begin
                            load_byte_ctr <= load_byte_ctr + 1;
                        end
                    end
                endcase
            end
        end
    end

    // SYS ARRAY
    sys_array_controller #()
    _sys_array_controller ();

    // MEMORY + UART
    reg write_valid_bmem;
    block_mem #(BMEM_ADDRSIZE, BITWIDTH, MESHUNITS, TILEUNITS)
    _block_mem (
        .clock(clock),
        .reset(reset),
        // TODO: add remaining signals
        .loader_write_addr(loader_addr_buffer),
        .loader_write_valid(write_valid_bmem),
        .loader_write_data(loader_bmem_data_buffer)
    );

    reg write_valid_imem0;
    imem #(IMEM_ADDRSIZE, BITWIDTH)
    _imem0 (
        .clock(clock),
        .reset(reset),
        .read_addr1(),
        .read_addr2(),
        .read_instr1(),
        .read_instr2(),
        .write_addr(loader_addr_buffer),
        .write_data(loader_imem_data_buffer),
        .write_valid(write_valid_imem0)
    );

    reg write_valid_imem1;
    imem #(IMEM_ADDRSIZE, BITWIDTH)
    _imem1 (
        .clock(clock),
        .reset(reset),
        .read_addr1(),
        .read_addr2(),
        .read_instr1(),
        .read_instr2(),
        .write_addr(loader_addr_buffer),
        .write_data(loader_imem_data_buffer),
        .write_valid(write_valid_imem1)
    );

    reg write_valid_imem2;
    imem #(IMEM_ADDRSIZE, BITWIDTH)
    _imem2 (
        .clock(clock),
        .reset(reset),
        .read_addr1(),
        .read_addr2(),
        .read_instr1(),
        .read_instr2(),
        .write_addr(loader_addr_buffer),
        .write_data(loader_imem_data_buffer),
        .write_valid(write_valid_imem2)
    );

    // TODO: UART: sync signals

    // TODO: UART: write signals

    // UART: read signals
    reg try_read;
    reg [7:0] read_data;
    reg read_data_valid;
    uart_controller #(BAUD_RATE, CLOCK_FREQ, BUFFER_SIZE)
    _uart_controller (
        .clock(clock),
        .reset(reset),
        .write_lock_req(),
        .write_lock_res(),
        .write_ready(),
        .data_in(),
        .data_in_valid(),
        .read_valid(1),
        .data_out(read_data),
        .data_out_valid(read_data_valid),
        .serial_in(serial_in),
        .serial_out(serial_out)
    );
    

endmodule
