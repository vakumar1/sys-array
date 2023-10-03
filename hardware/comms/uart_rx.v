module uart_receiver
    #(parameter SYMBOL_EDGE_TIME)
    (
        input clock,
        input reset,
        output [7:0] data_out,
        output data_out_valid,

        input serial_in,
        output rts
    );

endmodule