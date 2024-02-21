module thread
    #(
        parameter BITWIDTH, MESHUNITS, TILEUNITS
    )
    (
        input clock,
        input reset,
        input running,
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
        output write_data_valid

    );

    // instructions
    localparam
        TERMINATE = 2'b00,
        WRITE = 2'b01;

    // state
    localparam
        THREAD_DONE = 4'd0,
        THREAD_READ_INST = 4'd1,
        THREAD_WRITE_LOCK = 4'd2,
        THREAD_WRITE_BYTECOUNT = 4'd3,
        THREAD_WRITE_HEADER = 4'd4,
        THREAD_WRITE_DATA = 4'd5;
    reg [3:0] thread_state;

    // PC - stores current instruction counter
    reg [BITWIDTH-1:0] pc;
    assign imem_addr = pc;
        
    // WRITE instruction: signals + data
    // metadata + counters
    reg [BITWIDTH-1:0] write_byte_ctr;
    reg [BITWIDTH-1:0] write_bmem_idx_ctr;
    localparam BLOCK_SIZE = MESHUNITS * MESHUNITS * TILEUNITS * TILEUNITS;
    localparam [BITWIDTH-1:0] WRITE_BYTECOUNT = 1 + BITWIDTH * BLOCK_SIZE;

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
    always @(posedge clock) begin
        if (reset) begin
            thread_state <= THREAD_DONE;
            write_lock_req_buf <= 0;
        end
        else begin
            /* verilator lint_off CASEINCOMPLETE */
            case (thread_state)
                THREAD_DONE: begin
                    if (running)
                        thread_state <= THREAD_READ_INST;
                end
                THREAD_READ_INST: begin
                    if (~running) begin
                        thread_state <= THREAD_DONE;
                    end
                    else begin
                        /* verilator lint_off CASEINCOMPLETE */
                        case (imem_data[1:0])
                            TERMINATE: begin
                                thread_state <= THREAD_DONE;
                            end
                            WRITE: begin
                                // start WRITE instruction (get bmem addr to write from)
                                thread_state <= THREAD_WRITE_LOCK;
                                write_header <= imem_data[17:10];
                                write_bmem_addr <= {24'b0, imem_data[9:2]} << 8;
                                write_byte_ctr <= 0;
                                write_bmem_idx_ctr <= 0;
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
                // |32 -- 18|17 -- 10|9 --   2|1 --   0|
                // 
                THREAD_WRITE_LOCK: begin
                    // request write lock and proceed once acquired
                    write_lock_req_buf <= 1;
                    write_data_valid_buf <= 0;
                    write_byte_ctr <= write_byte_ctr + 1;
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
                            write_byte_ctr <= 0;
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
                        if (write_byte_ctr == 4) begin
                            // update index/byte counters
                            write_byte_ctr <= 0;
                            write_bmem_idx_ctr <= write_bmem_idx_ctr + 1;

                            // complete bmem data write (and relinquish write lock)
                            if (write_bmem_idx_ctr == BLOCK_SIZE) begin
                                write_lock_req_buf <= 0;
                                if (~write_lock_res) begin
                                    pc <= pc + 4;
                                    thread_state <= THREAD_READ_INST;
                                end
                            end
                        end

                        // write a byte from bmem data to UART
                        else begin
                            write_data_buf <= bmem_data[write_bmem_idx_ctr][8 * (write_byte_ctr) +: 8];
                            write_data_valid_buf <= 1;
                            write_byte_ctr <= write_byte_ctr + 1;
                        end
                    end
                    else begin
                        write_data_valid_buf <= 0;
                    end
                end

            endcase
        end
    end
    assign idle = thread_state == THREAD_DONE;
    
endmodule
