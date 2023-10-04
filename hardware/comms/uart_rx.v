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
        READING = 2'b01,
        FINISH = 2'b10;

    // update tick counter on each clock edge
    reg [31:0] tick_ctr;
    always @(posedge clock) begin
        if (reset)
            tick_ctr <= 0;
        else
            tick_ctr <= tick_ctr == SYMBOL_EDGE_TIME - 1 ? 0 : tick_ctr + 1;
    end

    // update receiver state on each symbol edge
    reg [1:0] state;
    reg [2:0] bit_pos;
    reg [7:0] buffer;
    always @(posedge clock) begin
        if (reset) begin
            state <= WAITING;
        end
        else if (tick_ctr == SYMBOL_EDGE_TIME - 1) begin            
            case (state)
                WAITING: begin
                    if (rts && !serial_in) begin
                        state <= READING;
                        bit_pos <= 7;
                    end
                end
                READING: begin
                    if (bit_pos == 0)
                        state <= FINISH;
                    if (bit_pos > 0)
                        bit_pos <= bit_pos - 1;
                    buffer[bit_pos] <= serial_in;
                end
                FINISH: begin
                    state <= WAITING;
                end
                default: begin
                    state <= WAITING;
                end
            endcase
        end
    end
    assign data_out = buffer;
    assign data_out_valid = state == FINISH;
    assign rts = (state == WAITING) && uart_ready;
endmodule
