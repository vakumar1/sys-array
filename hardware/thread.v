module thread
    #(
        parameter BITWIDTH, MESHUNITS, TILEUNITS
    )
    (
        input clock,
        input reset,
        input running,
        input idx,

        // reading from imem
        output [BITWIDTH-1:0] imem_addr,
        input [BITWIDTH-1:0] imem_data,

        // reading from bmem
        output [BITWIDTH-1:0] bmem_addr,
        input [(MESHUNITS * MESHUNITS * TILEUNITS * TILEUNITS)-1:0] bmem_data,

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
        THREAD_WRITE_ADDR = 4'd4,
        THREAD_WRITE_DATA = 4'd5,
    reg [3:0] thread_state;

    // TODO: add PC management
    reg [BITWIDTH-1:0] pc;
    
    // write signals + data
    reg [BITWIDTH-1:0] write_byte_ctr;
    localparam BLOCK_SIZE = MESHUNITS * MESHUNITS * TILEUNITS * TILEUNITS;
    localparam [BITWIDTH-1:0] WRITE_BYTECOUNT = 4 + BLOCK_SIZE;
    reg [BITWIDTH-1:0] write_bmem_addr;
    reg [BLOCK_SIZE-1:0] write_bmem_data;
    assign bmem_addr = write_bmem_addr;
    always @(posedge clock) begin
        if (reset) begin
            thread_state <= THREAD_DONE;
            write_lock_req <= 0;
        end
        else begin
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
                        case (imem_data[1:0])
                            TERMINATE: begin
                                thread_state <= THREAD_DONE;
                            end
                            WRITE: begin
                                // start write instruction (get bmem addr to write from)
                                thread_state <= THREAD_WRITE_LOCK;
                                write_bmem_addr <= (imem_data >> 2) << 2;
                                write_byte_ctr <= 0;
                            end
                        endcase
                    end
                end
                THREAD_WRITE_LOCK: begin
                    // request write lock and proceed once acquired
                    write_lock_req <= 1;
                    write_data_valid <= 0;
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
                            thread_state <= THREAD_WRITE_ADDR;
                            write_byte_ctr <= 0;
                        end

                        // write a byte from bytecount to UART
                        else begin
                            write_data <= WRITE_BYTECOUNT[8 * (write_byte_ctr) +: 8]
                            write_data_valid <= 1;
                            write_byte_ctr <= write_byte_ctr + 1;
                        end
                    end
                    else begin
                        write_data_valid <= 0;
                    end
                end
                THREAD_WRITE_ADDR: begin
                    if (write_ready) begin
                        // complete bmem addr write
                        if (write_byte_ctr == 4) begin
                            thread_state <= THREAD_WRITE_DATA;
                            write_byte_ctr <= 0;
                        end

                        // write a byte from bmem addr to UART
                        else begin
                            write_data <= write_bmem_addr[8 * (write_byte_ctr) +: 8]
                            write_data_valid <= 1;
                            write_byte_ctr <= write_byte_ctr + 1;
                        end
                    end
                    else begin
                        write_data_valid <= 0;
                    end
                end
                THREAD_WRITE_DATA: begin
                    if (write_ready) begin
                        // complete bmem data write
                        if (write_byte_ctr == BLOCK_SIZE) begin
                            write_lock_req <= 0;
                            if (~write_lock_res) <= 0 begin
                                thread_state <= THREAD_READ_INST;
                            end
                        end

                        // write a byte from bmem data to UART
                        else begin
                            write_data <= bmem_data[8 * (write_byte_ctr) +: 8]
                            write_data_valid <= 1;
                            write_byte_ctr <= write_byte_ctr + 1;
                        end
                    end
                    else begin
                        write_data_valid <= 0;
                    end
                end

            endcase
        end
    end
    assign imem_addr = pc;


endmodule