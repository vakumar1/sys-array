module uart
    #(parameter BAUD_RATE=115_200, CLOCK_FREQ=125_000_000)
    (
        input clock,
        input reset,

        // TX inputs
        input [7:0] data_in,
        input data_in_valid,

        // RX inputs
        output [7:0] data_out,
        output data_out_valid,

        // serial lines
        input serial_in,
        output serial_out,

        // flow control
        input cts, // CTS (remote RX is clear to send)
        output rts // RTS (local RX requests to send)
    );

    localparam SYMBOL_EDGE_TIME = CLOCK_FREQ / BAUD_RATE;

    uart_transmitter #(SYMBOL_EDGE_TIME)
    uart_tx (
        .clock(clock),
        .reset(reset),
        .data_in(data_in),
        .data_in_valid(data_in_valid),
        .serial_out(serial_out),
        .cts(cts)
    )

    uart_receiver #(SYMBOL_EDGE_TIME)
    uart_rx (
        .clock(clock),
        .reset(reset),
        .data_out(data_out),
        .data_out_valid(data_out_valid),
        .serial_in(serial_in),
        .rts(rts)
    );

endmodule
