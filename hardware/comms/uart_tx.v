module uart_transmitter
    #(parameter SYMBOL_EDGE_TIME)
    (
        input clock,
        input reset,
        input [7:0] data_in,
        input data_in_valid,

        output serial_out,
        input cts,

        output tx_running
    );

    parameter 
        WAITING = 2'b00,
        START = 2'b01,
        WRITING = 2'b10,
        FINISH = 2'b11;

    // update tick counter on each clock edge
    reg [31:0] tick_ctr;
    always @(posedge clock) begin
        if (reset)
            tick_ctr <= 0;
        else
            tick_ctr <= tick_ctr == SYMBOL_EDGE_TIME - 1 ? 0 : tick_ctr + 1;
    end

    // update transmitter state on each symbol edge
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
                    if (data_in_valid & cts) begin
                        state <= START;
                        buffer <= data_in;
                    end
                end
                START: begin
                    state <= WRITING;
                    bit_pos <= 7;
                end
                WRITING: begin
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
    end
    
    // assign serial out based on state + bit_pos
    assign serial_out = 
        state == WAITING        ? 1
            : state == START    ? 0
            : state == WRITING  ? buffer[bit_pos]
            : state == FINISH   ? 0
            : 1;
    assign tx_running = state != WAITING;

endmodule

