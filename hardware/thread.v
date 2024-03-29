module thread
    #(
        parameter BITWIDTH, MESHUNITS, TILEUNITS
    )
    (
        input clock,
        input reset,
        input start,
        input enabled,
        input idx,
        output idle, // UNUSED

        // reading from imem
        output [BITWIDTH-1:0] imem_addr,
        input [BITWIDTH-1:0] imem_data,

        // reading from bmem
        output [BITWIDTH-1:0] bmem_addr,
        input [BITWIDTH-1:0] bmem_data [(MESHUNITS * MESHUNITS * TILEUNITS * TILEUNITS)-1:0],

        // writing to UART: control signals
        output write_lock_req,
        input write_lock_res,
        input write_ready,

        // writing to UART: data signals
        output [7:0] write_data,
        output write_data_valid,

        // sysarray ctrl: load signals
        output [BITWIDTH-1:0] B_addr,
        output load_lock_req,
        input load_lock_res,
        input load_finished,

        // sysarray ctrl: comp signals
        output [BITWIDTH-1:0] A_addr,
        output [BITWIDTH-1:0] D_addr,
        output [BITWIDTH-1:0] C_addr,
        output comp_lock_req,
        input comp_lock_res,
        input comp_finished
    );

    // instructions
    localparam
        TERMINATE                       = 2'b00,
        WRITE                           = 2'b01,
        LOAD                            = 2'b10,
        COMP                            = 2'b11;

    // state
    localparam
        THREAD_DISABLED                 = 4'd0,
        THREAD_IDLE                     = 4'd1,
        THREAD_READ_INST                = 4'd2,

        // WRITE instr.
        THREAD_WRITE_ACQ_LOCK           = 4'd3,
        THREAD_WRITE_BYTECOUNT          = 4'd4,
        THREAD_WRITE_HEADER             = 4'd5,
        THREAD_WRITE_DATA               = 4'd6,
        THREAD_WRITE_REL_LOCK           = 4'd7,

        // LOAD instr.
        THREAD_LOAD_ACQ_LOCK            = 4'd8,
        THREAD_LOAD_WAIT                = 4'd9,
        THREAD_LOAD_REL_LOCK            = 4'd10,

        // COMP instr.
        THREAD_COMP_ACQ_LOCK            = 4'd11,
        THREAD_COMP_WAIT                = 4'd12,
        THREAD_COMP_REL_LOCK            = 4'd13;
    
    reg [3:0] thread_state /*verilator public*/;

    // PC - stores current instruction counter
    reg [BITWIDTH-1:0] pc;
    reg pc_reset_received;
    assign imem_addr = pc;
        
    // WRITE instruction: signals + data
    // metadata + counters
    reg [BITWIDTH-1:0] write_byte_ctr;
    reg [BITWIDTH-1:0] write_bmem_idx_ctr;
    localparam BLOCK_SIZE = MESHUNITS * MESHUNITS * TILEUNITS * TILEUNITS;
    localparam [BITWIDTH-1:0] WRITE_BYTECOUNT = 1 + 4 * BLOCK_SIZE;

    // synchronization signals
    reg write_lock_req_buf;
    assign write_lock_req = write_lock_req_buf;

    // addrs + data
    reg [7:0] write_header;
    reg [BITWIDTH-1:0] write_bmem_addr;
    reg [BITWIDTH-1:0] write_bmem_data [BLOCK_SIZE-1:0];
    reg [7:0] write_data_buf;
    reg write_data_valid_buf;
    assign bmem_addr = write_bmem_addr;
    assign write_data = write_data_buf;
    assign write_data_valid = write_data_valid_buf;

    // LOAD instruction: signals + data
    // synchronization signals
    reg load_lock_req_buf;
    assign load_lock_req = load_lock_req_buf;

    // addrs
    reg [BITWIDTH-1:0] B_addr_buf;
    assign B_addr = B_addr_buf;

    // COMP instruction: signals + data
    reg comp_lock_req_buf;
    assign comp_lock_req = comp_lock_req_buf;

    // addrs
    reg [BITWIDTH-1:0] A_addr_buf;
    reg [BITWIDTH-1:0] D_addr_buf;
    reg [BITWIDTH-1:0] C_addr_buf;
    assign A_addr = A_addr_buf;
    assign D_addr = D_addr_buf;
    assign C_addr = C_addr_buf;

    always @(posedge clock) begin
        if (reset) begin
            thread_state <= THREAD_IDLE;
            write_lock_req_buf <= 0;
            write_data_buf <= 0;
            write_data_valid_buf <= 0;
            pc <= 0;
            pc_reset_received <= 0;
        end
        else begin
            // accumulator for whether a start signal was received
            // used on next PC set
            pc_reset_received <= pc_reset_received | start;

            /* verilator lint_off CASEINCOMPLETE */
            case (thread_state)
                THREAD_DISABLED: begin
                    if (enabled) begin
                        thread_state <= THREAD_IDLE;
                    end
                end
                THREAD_IDLE: begin
                    if (~enabled) begin
                        thread_state <= THREAD_DISABLED;
                    end
                    else if (pc_reset_received | start) begin
                        thread_state <= THREAD_READ_INST;
                        pc <= 0;
                        pc_reset_received <= 0;
                    end
                end
                THREAD_READ_INST: begin
                    if (~enabled) begin
                        thread_state <= THREAD_DISABLED;
                    end
                    else begin
                        /* verilator lint_off CASEINCOMPLETE */
                        case (imem_data[1:0])
                            TERMINATE: begin
                                thread_state <= THREAD_IDLE;
                            end
                            WRITE: begin
                                // start WRITE instruction (get bmem addr to write from)
                                thread_state <= THREAD_WRITE_ACQ_LOCK;
                                write_header <= imem_data[17:10];
                                write_bmem_addr <= {24'b0, imem_data[9:2]} << 8;

                                // send write lock req signal 
                                // + initialize byte counter to read bmem data on next tick
                                write_lock_req_buf <= 1;
                                write_byte_ctr <= 0;
                            end
                            LOAD: begin
                                // start LOAD instruction
                                thread_state <= THREAD_LOAD_ACQ_LOCK;
                                B_addr_buf <= {24'b0, imem_data[9:2]} << 8;

                                // send load lock req signal
                                load_lock_req_buf <= 1;
                            end
                            COMP: begin
                                // start COMP instruction
                                thread_state <= THREAD_COMP_ACQ_LOCK;
                                A_addr_buf <= {24'b0, imem_data[9:2]} << 8;
                                D_addr_buf <= {24'b0, imem_data[17:10]} << 8;
                                C_addr_buf <= {24'b0, imem_data[25:18]} << 8;   
                                
                                // send comp lock req signal
                                comp_lock_req_buf <= 1;
                            end
                        endcase
                    end
                end

                // WRITE->UART instruction: writes
                // i. bytecount (2 + BLOCKSIZE)
                // ii. 2B metadata header
                // iii. BLOCKSIZE block starting at (addr << 8)
                // to UART as specified by 32b instruction:
                //
                // |unused  |header  |addr    |code    |
                // |(14)    |(8)     |(8)     |(2)     |   
                //    
                // |31 -- 18|17 -- 10|9 --   2|1 --   0|
                // 
                THREAD_WRITE_ACQ_LOCK: begin
                    // request write lock and proceed once acquired
                    if (write_lock_res) begin
                        thread_state <= THREAD_WRITE_BYTECOUNT;
                        write_byte_ctr <= 0;
                    end

                    // also read in bmem data to local reg
                    if (write_byte_ctr == 0) begin
                        write_bmem_data <= bmem_data;
                    end
                end
                THREAD_WRITE_BYTECOUNT: begin
                    if (write_ready) begin
                        // complete bytecount write
                        if (write_byte_ctr == 4) begin
                            thread_state <= THREAD_WRITE_HEADER;
                            write_data_valid_buf <= 0;
                            write_byte_ctr <= 0;
                        end

                        // write a byte from bytecount to UART
                        else begin
                            write_data_buf <= WRITE_BYTECOUNT[8 * (write_byte_ctr) +: 8];
                            write_data_valid_buf <= 1;
                            write_byte_ctr <= write_byte_ctr + 1;
                        end
                    end
                    else begin
                        write_data_valid_buf <= 0;
                    end
                end
                THREAD_WRITE_HEADER: begin
                    if (write_ready) begin
                        // complete 1B header write
                        if (write_byte_ctr == 1) begin
                            thread_state <= THREAD_WRITE_DATA;
                            write_data_valid_buf <= 0;
                            write_byte_ctr <= 0;
                            write_bmem_idx_ctr <= 0;
                        end

                        // write header byte to UART
                        else begin
                            write_data_buf <= write_header;
                            write_data_valid_buf <= 1;
                            write_byte_ctr <= write_byte_ctr + 1;
                        end
                    end
                    else begin
                        write_data_valid_buf <= 0;
                    end
                end
                THREAD_WRITE_DATA: begin
                    if (write_ready) begin
                        // complete bmem word write
                        if (write_bmem_idx_ctr == BLOCK_SIZE) begin
                            thread_state <= THREAD_WRITE_REL_LOCK;
                            write_lock_req_buf <= 0;
                            write_data_valid_buf <= 0;
                        end

                        // write a byte from bmem data to UART
                        else begin
                            write_data_buf <= bmem_data[write_bmem_idx_ctr][8 * (write_byte_ctr) +: 8];
                            write_data_valid_buf <= 1;
                            write_byte_ctr <= write_byte_ctr == 3 ? 0 : write_byte_ctr + 1;
                            write_bmem_idx_ctr <= write_byte_ctr == 3 ? write_bmem_idx_ctr + 1 : write_bmem_idx_ctr;
                        end
                    end
                    else begin
                        write_data_valid_buf <= 0;
                    end
                end
                THREAD_WRITE_REL_LOCK: begin
                    if (~write_lock_res) begin
                        thread_state <= THREAD_READ_INST;
                        pc <= pc_reset_received | start ? 0 : pc + 4;
                        pc_reset_received <= 0;
                    end
                end

                // LOAD instruction: submits B_addr to sys array ctrl
                // and synchronously waits until load completes
                //
                // |unused  |B_addr  |code    |
                // |(22)    |(8)     |(2)     |   
                //    
                // |31 -- 10|9 --   2|1 --   0|
                //
                THREAD_LOAD_ACQ_LOCK: begin
                    // request load lock and proceed once acquired
                    if (load_lock_res) begin
                        thread_state <= THREAD_LOAD_WAIT;
                    end
                end
                THREAD_LOAD_WAIT: begin
                    if (load_finished) begin
                        thread_state <= THREAD_LOAD_REL_LOCK;
                        load_lock_req_buf <= 0;
                    end
                end
                THREAD_LOAD_REL_LOCK: begin
                    if (~load_lock_res) begin
                        thread_state <= THREAD_READ_INST;
                        pc <= pc_reset_received | start ? 0 : pc + 4;
                        pc_reset_received <= 0;
                    end
                end

                // COMP instruction: submits A, C, and D addrs to sys array ctrl
                // and synchronously waits until comp completes
                //
                // |unused  |C_addr  |D_addr  |A_addr  |code    |
                // |(6)     |(8)     |(8)     |(8)     |(2)     |
                //    
                // |31 -- 26|25 -- 18|17 -- 10|9 --   2|1 --   0|
                //
                THREAD_COMP_ACQ_LOCK: begin
                    // request COMP lock and proceed once acquired
                    if (comp_lock_res) begin
                        thread_state <= THREAD_COMP_WAIT;
                    end
                end
                THREAD_COMP_WAIT: begin
                    if (comp_finished) begin
                        thread_state <= THREAD_COMP_REL_LOCK;
                        comp_lock_req_buf <= 0;
                    end
                end
                THREAD_COMP_REL_LOCK: begin
                    if (~comp_lock_res) begin
                        thread_state <= THREAD_READ_INST;
                        pc <= pc_reset_received | start ? 0 : pc + 4;
                        pc_reset_received <= 0;
                    end
                end
            endcase
        end
    end
    assign idle = thread_state == THREAD_IDLE;
    
endmodule
