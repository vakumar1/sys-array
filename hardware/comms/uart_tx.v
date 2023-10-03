module uart_transmitter
    #(parameter SYMBOL_EDGE_TIME)
    (
        input clock,
        input reset,
        input [7:0] data_in,
        input data_in_valid,

        output serial_out,
        input cts
    );

    parameter 
        WAITING = 2'b00,
        START = 2'b01,
        WRITING = 2'b10,
        FINISH = 2'b11;

    // update tick counter on each clock edge
    reg [$clog2(SYMBOL_EDGE_TIME):0] tick_ctr;
    always @(posedge clock) begin
        if (reset)
            tick_ctr <= 0;
        else
            tick_ctr <= tick_ctr == SYMBOL_EDGE_TIME - 1 ? 0 : tick_ctr + 1;
    end

    // update transmitter state on each symbol edge
    reg [1:0] state;
    reg [3:0] bit_pos;
    reg [7:0] buffer;
    reg out_bit;
    always @(posedge clock) begin
        if (reset) begin
            state <= WAITING;
            bit_pos <= 0;
            buffer <= 0;
            out_bit <= 1;
        end
        else if (tick_ctr == SYMBOL_EDGE_TIME - 1) begin            
            case (state)
                WAITING: begin
                    if (data_in_valid & cts) begin
                        state <= START;
                        buffer <= data_in;
                    end
                    out_bit <= 1;
                end
                START: begin
                    state <= WRITING;
                    bit_pos <= 7;
                    out_bit <= 0;
                end
                WRITING: begin
                    if (bit_pos == 0)
                        state <= FINISH;
                    if (bit_pos > 0)
                        bit_pos <= bit_pos - 1;
                    out_bit <= buffer[bit_pos];
                end
                FINISH: begin
                    state <= WAITING;
                    out_bit <= 0;
                end
            endcase
        end
    end
    assign serial_out = out_bit;

endmodule

