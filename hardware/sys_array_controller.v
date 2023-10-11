module sys_array_controller
    #(parameter MESHROWS, MESHCOLS, BITWIDTH, TILEROWS, TILECOLS)
    (
        input clock,
        input reset,

        // CONTROL SIGNALS
        input comp_lock_req [1:0],
        input [15:0] A_addr [1:0],
        input [15:0] D_addr [1:0],
        input [15:0] C_addr [1:0],
        output comp_lock_res [1:0], // will never have "1"s overlap with load_lock_res
        output comp_finished,

        input load_lock_req [1:0],
        input [15:0] b_addr [1:0],
        output load_lock_res [1:0], // will never have "1"s overlap with comp_lock_res
        output load_finished,

        // MEMORY READ SIGNALS
        input [31:0] A [MESHROWS-1:0][TILEROWS-1:0],
        input [31:0] D [MESHCOLS-1:0][TILECOLS-1:0],
        input [31:0] B [MESHCOLS-1:0][TILECOLS-1:0],
        output [15:0] A_row_read_addrs [MESHROWS-1:0],
        output [15:0] D_col_read_addrs [MESHCOLS-1:0],
        output [15:0] B_col_read_addrs [MESHCOLS-1:0],

        // MEMORY WRITE SIGNALS
        output [31:0] C [MESHCOLS-1:0][TILECOLS-1:0],
        output [15:0] C_col_write_addrs [MESHCOLS-1:0];
        output C_write_valid [MESHCOLS-1],
    );


    sys_array #(parameter MESHROWS=MESHROWS, MESHCOLS=MESHCOLS, BITWIDTH=BITWIDTH, TILEROWS=TILEROWS, TILECOLS=TILECOLS)
    sys_array_module (
        .clock(clock),
        .reset(reset),
        .in_dataflow(),
        .in_a(),
        .in_a_valid(),
        .in_b(),
        .in_d(),
        .in_propagate(),
        .in_b_shelf_life(),
        .in_b_valid(),
        .in_d_valid(),
        .out_c(),
        .out_c_valid()
    );


endmodule