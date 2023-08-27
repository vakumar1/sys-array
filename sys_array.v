module sys_array
    #(parameter MESHROWS, MESHCOLUMNS, BITWIDTH, TILEROWS=1, TILECOLUMNS=1)
    (
        input                               clock,
        input                               reset,
        input signed [BITWIDTH-1:0]   in_a[MESHROWS-1:0][TILEROWS-1:0],
        input signed [BITWIDTH-1:0]   in_d[MESHCOLUMNS-1:0][TILECOLUMNS-1:0],
        input signed [BITWIDTH-1:0]   in_b[MESHCOLUMNS-1:0][TILECOLUMNS-1:0],
        input                               in_dataflow[MESHCOLUMNS-1:0][TILECOLUMNS-1:0],
        input                               in_propagate[MESHCOLUMNS-1:0][TILECOLUMNS-1:0],
        input                               in_valid[MESHCOLUMNS-1:0][TILECOLUMNS-1:0],
        output signed [BITWIDTH-1:0] out_c[MESHCOLUMNS-1:0][TILECOLUMNS-1:0],
        output signed [BITWIDTH-1:0] out_b[MESHCOLUMNS-1:0][TILECOLUMNS-1:0],
        output                              out_valid[MESHCOLUMNS-1:0][TILECOLUMNS-1:0]
    );

    // store intermediate outputs in wires
    reg signed [BITWIDTH-1:0] inter_a[MESHROWS-1:0][MESHCOLUMNS:0][TILEROWS-1:0] /*verilator split_var*/;
    reg signed [BITWIDTH-1:0] inter_b[MESHROWS:0][MESHCOLUMNS-1:0][TILECOLUMNS-1:0] /*verilator split_var*/;
    reg signed [BITWIDTH-1:0] inter_d[MESHROWS:0][MESHCOLUMNS-1:0][TILECOLUMNS-1:0] /*verilator split_var*/;
    reg signed inter_dataflow[MESHROWS:0][MESHCOLUMNS-1:0][TILECOLUMNS-1:0] /*verilator split_var*/;
    reg signed inter_propagate[MESHROWS:0][MESHCOLUMNS-1:0][TILECOLUMNS-1:0] /*verilator split_var*/;
    reg signed inter_valid[MESHROWS:0][MESHCOLUMNS-1:0][TILECOLUMNS-1:0] /*verilator split_var*/;

    // the first row (for north inputs) or col (for west inputs) is automatically assigned to be the input
    // the last row (for north inputs that are returned) is automatically assigned to be the output
    genvar k, l, t;
    generate
        for (k = 0; k < MESHROWS; k++) begin
            for (t = 0; t < TILEROWS; t++) begin
                assign inter_a[k][0][t] = in_a[k][t];
            end
        end
        for (l = 0; l < MESHCOLUMNS; l++) begin
            for (t = 0; t < TILECOLUMNS; t++) begin
                assign inter_b[0][l][t] = in_b[l][t];
                assign inter_d[0][l][t] = in_d[l][t];
                assign inter_dataflow[0][l][t] = in_dataflow[l][t];
                assign inter_propagate[0][l][t] = in_propagate[l][t];
                assign inter_valid[0][l][t] = in_valid[l][t];

                assign out_b[l][t] = inter_b[MESHROWS][l][t];
                assign out_c[l][t] = inter_d[MESHROWS][l][t];
                assign out_valid[l][t] = inter_valid[MESHROWS][l][t];
            end
        end
        endgenerate

    // generate Tile blocks and connect via intermediate registers
    // (connect edge Tiles blocks to inputs/outputs)
    genvar i, j;
    generate
        for (i = 0; i < MESHROWS; i++) begin
            for (j = 0; j < MESHCOLUMNS; j++) begin
                Tile #(BITWIDTH, TILEROWS, TILECOLUMNS) 
                tile_instance (
                    .clock(clock),
                    .reset(reset),
                    .in_a(inter_a[i][j]),
                    .in_b(inter_b[i][j]),
                    .in_d(inter_d[i][j]),
                    .in_dataflow(inter_dataflow[i][j]),
                    .in_propagate(inter_propagate[i][j]),
                    .in_valid(inter_valid[i][j]),
                    .out_a(inter_a[i][j + 1]),
                    .out_b(inter_b[i + 1][j]),
                    .out_c(inter_d[i + 1][j]),
                    .out_dataflow(inter_dataflow[i + 1][j]),
                    .out_propagate(inter_propagate[i + 1][j]),
                    .out_valid(inter_valid[i + 1][j])
                );

                always @(posedge clock) begin
                    inter_a[i][j + 1] <= tile_instance.out_a;
                    inter_b[i + 1][j] <= tile_instance.out_b;
                    inter_d[i + 1][j] <= tile_instance.out_c;
                    inter_dataflow[i + 1][j] <= tile_instance.out_dataflow;
                    inter_propagate[i + 1][j] <= tile_instance.out_propagate;
                    inter_valid[i + 1][j] <= tile_instance.out_valid;
                end
            end
        end
    endgenerate


endmodule

module Tile
    #(parameter BITWIDTH, TILEROWS, TILECOLUMNS)
    (
        input clock,
        input reset,
        input signed [BITWIDTH-1:0] in_a[TILEROWS-1:0],
        input signed [BITWIDTH-1:0] in_b[TILECOLUMNS-1:0],
        input signed [BITWIDTH-1:0] in_d[TILECOLUMNS-1:0],
        input in_dataflow[TILECOLUMNS-1:0],
        input in_propagate[TILECOLUMNS-1:0],
        input in_valid[TILECOLUMNS-1:0],
        output signed [BITWIDTH-1:0] out_a[TILECOLUMNS-1:0],
        output signed [BITWIDTH-1:0] out_b[TILECOLUMNS-1:0],
        output signed [BITWIDTH-1:0] out_c[TILECOLUMNS-1:0],
        output out_dataflow[TILECOLUMNS-1:0],
        output out_propagate[TILECOLUMNS-1:0],
        output out_valid[TILECOLUMNS-1:0]
    );

    // store intermediate outputs in wires
    wire signed [BITWIDTH-1:0] inter_a[TILEROWS-1:0][TILECOLUMNS:0] /*verilator split_var*/;
    wire signed [BITWIDTH-1:0] inter_b[TILEROWS:0][TILECOLUMNS-1:0] /*verilator split_var*/;
    wire signed [BITWIDTH-1:0] inter_d[TILEROWS:0][TILECOLUMNS-1:0] /*verilator split_var*/;
    wire signed inter_dataflow[TILEROWS:0][TILECOLUMNS-1:0] /*verilator split_var*/;
    wire signed inter_propagate[TILEROWS:0][TILECOLUMNS-1:0] /*verilator split_var*/;
    wire signed inter_valid[TILEROWS:0][TILECOLUMNS-1:0] /*verilator split_var*/;

    // the first row (for north inputs) or col (for west inputs) is automatically assigned to be the input
    // the last row (for north inputs) or col (for west inputs) is automatically assigned to be the output
    genvar k, l;
    generate
        for (k = 0; k < TILEROWS; k++) begin
            assign inter_a[k][0] = in_a[k];
            assign out_a[k] = inter_a[k][TILECOLUMNS];
        end
    endgenerate
    for (l = 0; l < TILECOLUMNS; l++) begin
        assign inter_b[0][l] = in_b[l];
        assign inter_d[0][l] = in_d[l];
        assign inter_dataflow[0][l] = in_dataflow[l];
        assign inter_propagate[0][l] = in_propagate[l];
        assign inter_valid[0][l] = in_valid[l];

        assign out_b[l] = inter_b[TILEROWS][l];
        assign out_c[l] = inter_d[TILEROWS][l];
        assign out_dataflow[l] = inter_dataflow[TILEROWS][l];
        assign out_propagate[l] = inter_propagate[TILEROWS][l];
        assign out_valid[l] = inter_valid[TILEROWS][l];
    end

    // generate PE blocks and connect via intermediate wires
    // (connect edge PE blocks to inputs/outputs)
    genvar i, j;
    generate
        for (i = 0; i < TILEROWS; i++) begin
            for (j = 0; j < TILECOLUMNS; j++) begin
                PE #(BITWIDTH) 
                pe_instance (
                    .clock(clock),
                    .reset(reset),
                    .in_a(inter_a[i][j]),
                    .in_b(inter_b[i][j]),
                    .in_d(inter_d[i][j]),
                    .in_dataflow(inter_dataflow[i][j]),
                    .in_propagate(inter_propagate[i][j]),
                    .in_valid(inter_valid[i][j]),
                    .out_a(inter_a[i][j + 1]),
                    .out_b(inter_b[i + 1][j]),
                    .out_c(inter_d[i + 1][j]),
                    .out_dataflow(inter_dataflow[i + 1][j]),
                    .out_propagate(inter_propagate[i + 1][j]),
                    .out_valid(inter_valid[i + 1][j])
                );
            end
        end
    endgenerate
endmodule


module PE
    #(parameter BITWIDTH)
    (
        input clock,
        input reset,
        input signed [BITWIDTH-1:0] in_a,
        input signed [BITWIDTH-1:0] in_b,
        input signed [BITWIDTH-1:0] in_d,
        input in_dataflow,
        input in_propagate,
        input in_valid,
        output signed [BITWIDTH-1:0] out_a,
        output signed [BITWIDTH-1:0] out_b,
        output signed [BITWIDTH-1:0] out_c,
        output out_dataflow,
        output out_propagate,
        output out_valid
    );

    reg signed [BITWIDTH-1:0] b0;
    reg signed [BITWIDTH-1:0] b1;
    reg valid0;
    reg valid1;

    always @(posedge clock) begin
        if (reset) begin
            valid0 <= 0;
            valid1 <= 0;
        end
        else begin
            b0 <= (!valid || in_propagate) ? b0 : in_b;
            b1 <= (!valid || !in_propagate) ? b1 : in_b;
            valid0 <= (!valid || in_propagate) ? valid0 : in_valid;
            valid1 <= (!valid || !in_propagate) ? valid1 : in_valid;
        end
    end
    
    assign out_a = in_a;
    assign out_b = (in_propagate ? b0 : b1);
    assign out_c = in_d + in_a * (in_propagate ? b0 : b1);
    assign out_dataflow = in_dataflow;
    assign out_propagate = in_propagate;
    assign out_valid = (in_propagate ? valid0 : valid1);

endmodule

