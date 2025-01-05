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
    always @(posedge clock) begin
        // default invalidate all loader write signals
        write_valid_bmem <= 0;
        write_valid_imem0 <= 0;
        write_valid_imem1 <= 0;

        if (reset) begin
            // -> LOADER START
            loader_state <= LOADER_START;
            thread0_start <= 0;
            thread0_enabled <= 0;
            thread1_start <= 0;
            thread1_enabled <= 0;
        end
        else begin
            // thread start signal defaults to 0
            // unless an actual update is received
            thread0_start <= 0;
            thread1_start <= 0;

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
                                thread0_start <= read_data[0];
                                thread0_enabled <= read_data[1];
                                thread1_start <= read_data[2];
                                thread1_enabled <= read_data[3];
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
                            if (~thread0_enabled) begin
                                write_valid_imem0 <= 1;
                            end
                            else if (~thread1_enabled) begin
                                write_valid_imem1 <= 1;
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

    // ARRAY WRITE SIGNALS <-> BMEM
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

        // THREAD -> BMEM READ
        .thread0_read_addr(thread0_bmem_addr),
        .thread0_read_data(thread0_bmem_data),

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
    reg [BITWIDTH-1:0] read_addr0_1;
    reg [BITWIDTH-1:0] read_instr0_1;
    imem #(IMEM_ADDRSIZE, BITWIDTH)
    _imem0 (
        .clock(clock),
        .reset(reset),
        .read_addr1(read_addr0_1),
        .read_addr2(),
        .read_instr1(read_instr0_1),
        .read_instr2(),
        .write_addr(loader_addr_buffer),
        .write_data(loader_imem_data_buffer),
        .write_valid(write_valid_imem0)
    );

    // TODO: associate IMEM with separate thread or remove
    reg write_valid_imem1;
    reg [BITWIDTH-1:0] read_addr1_1;
    reg [BITWIDTH-1:0] read_instr1_1;
    imem #(IMEM_ADDRSIZE, BITWIDTH)
    _imem1 (
        .clock(clock),
        .reset(reset),
        .read_addr1(read_addr1_1),
        .read_addr2(),
        .read_instr1(read_instr1_1),
        .read_instr2(),
        .write_addr(loader_addr_buffer),
        .write_data(loader_imem_data_buffer),
        .write_valid(write_valid_imem1)
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

    // THREADS
    reg thread0_start;
    reg thread0_enabled;
    reg thread1_start;
    reg thread1_enabled;

    reg [BITWIDTH-1:0] thread0_bmem_addr;
    reg [BITWIDTH-1:0] thread0_bmem_data [(MESHUNITS * MESHUNITS * TILEUNITS * TILEUNITS) - 1:0];
    thread #(BITWIDTH, MESHUNITS, TILEUNITS)
    _thread0 (
        // CONTROL SIGNALS
        .clock(clock),
        .reset(reset),
        .start(thread0_start),
        .enabled(thread0_enabled),
        .idx(0),
        .idle(),

        // MEM READ/WRITE SIGNALS
        .imem_addr(read_addr0_1),
        .imem_data(read_instr0_1),
        .bmem_addr(thread0_bmem_addr),
        .bmem_data(thread0_bmem_data),

        // UART
        .write_lock_req(write_lock_req[0]),
        .write_lock_res(write_lock_res[0]),
        .write_ready(write_ready),
        .write_data(write_data[0]),
        .write_data_valid(write_data_valid[0]),

        // SYSARRAY LOAD
        .B_addr(B_addr[0]),
        .load_lock_req(load_lock_req[0]),
        .load_lock_res(load_lock_res[0]),
        .load_finished(load_finished),

        // SYSARRAY COMP
        .A_addr(A_addr[0]),
        .D_addr(D_addr[0]),
        .C_addr(C_addr[0]),
        .comp_lock_req(comp_lock_req[0]),
        .comp_lock_res(comp_lock_res[0]),
        .comp_finished(comp_finished)
    );

    reg [BITWIDTH-1:0] thread1_bmem_addr;
    reg [BITWIDTH-1:0] thread1_bmem_data [(MESHUNITS * MESHUNITS * TILEUNITS * TILEUNITS) - 1:0];
    thread #(BITWIDTH, MESHUNITS, TILEUNITS)
    _thread1 (
        // CONTROL SIGNALS
        .clock(clock),
        .reset(reset),
        .start(thread1_start),
        .enabled(thread1_enabled),
        .idx(1),
        .idle(),

        // MEM READ/WRITE SIGNALS
        .imem_addr(read_addr1_1),
        .imem_data(read_instr1_1),
        .bmem_addr(thread1_bmem_addr),
        .bmem_data(thread1_bmem_data),

        // UART
        .write_lock_req(write_lock_req[1]),
        .write_lock_res(write_lock_res[1]),
        .write_ready(write_ready),
        .write_data(write_data[1]),
        .write_data_valid(write_data_valid[1]),

        // SYSARRAY LOAD
        .B_addr(B_addr[1]),
        .load_lock_req(load_lock_req[1]),
        .load_lock_res(load_lock_res[1]),
        .load_finished(load_finished),

        // SYSARRAY COMP
        .A_addr(A_addr[1]),
        .D_addr(D_addr[1]),
        .C_addr(C_addr[1]),
        .comp_lock_req(comp_lock_req[1]),
        .comp_lock_res(comp_lock_res[1]),
        .comp_finished(comp_finished)
    );
    

endmodule
