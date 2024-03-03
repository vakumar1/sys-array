module uart_receiver
    #(parameter SYMBOL_EDGE_TIME)
    (
        input clock,
        input reset,
        output [7:0] data_out,
        output data_out_valid,

        input serial_in,
        output rts,

        input uart_ready
    );
    
    parameter 
        WAITING = 2'b00,
        START = 2'b01,
        READING = 2'b10,
        FINISH = 2'b11;

    // wait until transmitter tells us to start --> start tick ctr and update state on each symbol edge
    reg [31:0] tick_ctr;
    reg [1:0] state;
    reg [2:0] bit_pos;
    reg [7:0] buffer /*verilator public*/;
    always @(posedge clock) begin
        if (reset) begin
            state <= WAITING;
            tick_ctr <= 0;
        end

        // wait until WAITING period is over
        // to sync up with transmitter and enter START
        else if (state == WAITING) begin
            if (rts && !serial_in) begin
                state <= START;
            end
            tick_ctr <= 0;
        end
        else begin
            // sample data read from middle of symbol cycle to reduce edge errors
            if (tick_ctr == SYMBOL_EDGE_TIME / 2 && state == READING)
                buffer[bit_pos] <= serial_in;
            if (tick_ctr == SYMBOL_EDGE_TIME - 1) begin            
                case (state)
                    START: begin
                        state <= READING;
                        bit_pos <= 7;
                    end
                    READING: begin
                        if (bit_pos == 0)
                            state <= FINISH;
                        if (bit_pos > 0)
                            bit_pos <= bit_pos - 1;
                        
                    end
                    FINISH: begin
                        state <= WAITING;
                    end
                    default: begin
                        state <= WAITING;
                    end
                endcase
            end
            tick_ctr <= tick_ctr == SYMBOL_EDGE_TIME - 1 ? 0 : tick_ctr + 1;
        end
    end
    assign data_out = buffer;
    assign data_out_valid = state == FINISH && tick_ctr == 0;
    assign rts = (state == WAITING) && uart_ready;
endmodule
