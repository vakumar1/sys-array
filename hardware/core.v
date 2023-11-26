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
        LOADER_START = 3'd0,
        LOADER_IMEM_ADDR = 3'd1,
        LOADER_IMEM_DATA = 3'd2,
        LOADER_BMEM_ADDR = 3'd3,
        LOADER_BMEM_DATA = 3'd4;

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
            thread0_running <= 0;
            thread1_running <= 0;
            thread2_running <= 0;
        end
        else begin
            if (read_data_valid) begin
                /* verilator lint_off CASEINCOMPLETE */
                case (loader_state)
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
                        loader_addr_buffer[8 * (loader_byte_ctr) +: 8] <= read_data;
                        if (loader_byte_ctr == (BITWIDTH >> 3) - 1) begin
                            // -> IMEM DATA
                            loader_state <= LOADER_IMEM_DATA;
                            loader_byte_ctr <= 0;
                        end
                        else begin
                            loader_byte_ctr <= loader_byte_ctr + 1;
                        end
                    end
                    LOADER_IMEM_DATA: begin
                        loader_imem_data_buffer[8 * (loader_byte_ctr) +: 8] <= read_data;
                        if (loader_byte_ctr == (BITWIDTH >> 3) - 1) begin
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
                            loader_state <= LOADER_START;
                        end
                        else begin
                            loader_byte_ctr <= loader_byte_ctr + 1;
                        end
                    end
                    LOADER_BMEM_ADDR: begin
                        loader_addr_buffer[8 * (loader_byte_ctr) +: 8] <= read_data;
                        if (loader_byte_ctr == (BITWIDTH >> 3) - 1) begin
                            // -> BMEM DATA
                            loader_state <= LOADER_BMEM_DATA;
                            loader_byte_ctr <= 0;
                        end
                        else begin
                            loader_byte_ctr <= loader_byte_ctr + 1;
                        end
                    end
                    LOADER_BMEM_DATA: begin
                        loader_bmem_data_buffer[(loader_byte_ctr >> 2)][8 * (loader_byte_ctr[1:0]) +: 8] <= read_data;
                        if (loader_byte_ctr == (BITWIDTH >> 3) * (MESHUNITS * MESHUNITS * TILEUNITS * TILEUNITS) - 1) begin
                            // write bmem data
                            write_valid_bmem <= 1;

                            // -> START
                            loader_state <= LOADER_START;
                        end
                        else begin
                            loader_byte_ctr <= loader_byte_ctr + 1;
                        end
                    end
                endcase
            end
        end
    end

    // THREADS
    wire thread0_idx = 0;
    wire thread1_idx = thread0_running ? 1 : 0;
    wire thread2_idx = (thread0_running | thread1_running) ? 1 : 0;

    // SYS ARRAY
    // COMP LOGIC SIGNALS <-> THREADS
    reg comp_lock_req [1:0];
    reg comp_lock_res [1:0];
    reg comp_finished;
    reg [BITWIDTH-1:0] A_addr [1:0];
    reg [BITWIDTH-1:0] D_addr [1:0];
    reg [BITWIDTH-1:0] C_addr [1:0];

    // LOAD LOGIC SIGNALS <-> THREADS
    reg load_lock_req [1:0];
    reg load_lock_res [1:0];
    reg load_finished;
    reg [BITWIDTH-1:0] B_addr [1:0];

    // ARRAY READ SIGNALS <-> BMEM
    wire [BITWIDTH-1:0] A [MESHUNITS-1:0][TILEUNITS-1:0];
    wire [BITWIDTH-1:0] D [MESHUNITS-1:0][TILEUNITS-1:0];
    wire [BITWIDTH-1:0] B [MESHUNITS-1:0][TILEUNITS-1:0];
    wire [BITWIDTH-1:0] A_row_read_addrs [MESHUNITS-1:0];
    wire [BITWIDTH-1:0] D_col_read_addrs [MESHUNITS-1:0];
    wire [BITWIDTH-1:0] B_col_read_addrs [MESHUNITS-1:0];
    wire A_read_valid [MESHUNITS-1:0];
    wire D_read_valid [MESHUNITS-1:0];
    wire B_read_valid [MESHUNITS-1:0];

    // ARRAY WRITE SINGALS <-> BMEM
    wire [BITWIDTH-1:0] C [MESHUNITS-1:0][TILEUNITS-1:0];
    wire [BITWIDTH-1:0] C_col_write_addrs [MESHUNITS-1:0];
    wire C_write_valid [MESHUNITS-1:0];

    sys_array_controller #(BITWIDTH, MESHUNITS, TILEUNITS)
    _sys_array_controller (
        .clock(clock),
        .reset(reset),

        // COMP LOGIC
        .comp_lock_req(comp_lock_req),
        .A_addr(A_addr),
        .D_addr(D_addr),
        .C_addr(C_addr),
        .comp_lock_res(comp_lock_res),
        .comp_finished(comp_finished),

        // LOAD LOGIC
        .load_lock_req(load_lock_req),
        .B_addr(B_addr),
        .load_lock_res(load_lock_res),
        .load_finished(load_finished),

        // MEMORY READ SIGNALS
        .A(A),
        .D(D),
        .B(B),
        .A_row_read_addrs(A_row_read_addrs),
        .D_col_read_addrs(D_col_read_addrs),
        .B_col_read_addrs(B_col_read_addrs),
        .A_read_valid(A_read_valid),
        .D_read_valid(D_read_valid),
        .B_read_valid(B_read_valid),

        // MEMORY WRITE SIGNALS
        .C(C),
        .C_col_write_addrs(C_col_write_addrs),
        .C_write_valid(C_write_valid)
    );

    // MEMORY + UART
    reg write_valid_bmem;
    blockmem #(BMEM_ADDRSIZE, BITWIDTH, MESHUNITS, TILEUNITS)
    _blockmem (
        .clock(clock),
        .reset(reset),

        // ARRAY -> BMEM READ
        .A_tile_read_addrs(A_row_read_addrs),
        .D_tile_read_addrs(D_col_read_addrs),
        .B_tile_read_addrs(B_col_read_addrs),
        .A_read_valid(A_read_valid),
        .D_read_valid(D_read_valid),
        .B_read_valid(B_read_valid),
        .A(A),
        .D(D),
        .B(B),

        // ARRAY -> BMEM WRITE
        .C_tile_write_addrs(C_col_write_addrs),
        .C_write_valid(C_write_valid),
        .C(C),

        // LOADER -> BMEM WRITE
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

    // UART: write sync + signals
    reg write_lock_req [1:0];
    reg write_lock_res [1:0];
    reg [7:0] write_data [1:0];
    reg write_data_valid [1:0];
    reg write_ready;

    // UART: read signals
    reg [7:0] read_data;
    reg read_data_valid;
    uart_controller #(BAUD_RATE, CLOCK_FREQ, BUFFER_SIZE)
    _uart_controller (
        .clock(clock),
        .reset(reset),
        .write_lock_req(write_lock_req),
        .write_lock_res(write_lock_res),
        .write_ready(write_ready),
        .data_in(write_data),
        .data_in_valid(write_data_valid),
        .read_valid(1),
        .data_out(read_data),
        .data_out_valid(read_data_valid),
        .serial_in(serial_in),
        .serial_out(serial_out)
    );
    

endmodule
