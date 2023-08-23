module Mesh
    #(parameter MESHROWS, MESHCOLUMNS, BITWIDTH, TILEROWS=1, TILECOLUMNS=1)
    (
        input                               clock,
        input                               reset,
        input signed [BITWIDTH-1:0]   in_a[MESHROWS-1:0][TILEROWS-1:0],
        input signed [BITWIDTH-1:0]   in_d[MESHCOLUMNS-1:0][TILECOLUMNS-1:0],
        input signed [BITWIDTH-1:0]   in_b[MESHCOLUMNS-1:0][TILECOLUMNS-1:0],
        input                               in_control_dataflow[MESHCOLUMNS-1:0][TILECOLUMNS-1:0],
        input                               in_control_propagate[MESHCOLUMNS-1:0][TILECOLUMNS-1:0],
        input                               in_valid[MESHCOLUMNS-1:0][TILECOLUMNS-1:0],
        output signed [BITWIDTH-1:0] out_c[MESHCOLUMNS-1:0][TILECOLUMNS-1:0],
        output signed [BITWIDTH-1:0] out_b[MESHCOLUMNS-1:0][TILECOLUMNS-1:0],
        output                              out_valid[MESHCOLUMNS-1:0][TILECOLUMNS-1:0]
    );

    // store intermediate outputs in wires
    reg signed [BITWIDTH-1:0] inter_a[MESHROWS-1:0][MESHCOLUMNS:0][TILEROWS-1:0];
    reg signed [BITWIDTH-1:0] inter_b[MESHROWS:0][MESHCOLUMNS-1:0][TILECOLUMNS-1:0];
    reg signed [BITWIDTH-1:0] inter_d[MESHROWS:0][MESHCOLUMNS-1:0][TILECOLUMNS-1:0];
    reg signed inter_dataflow[MESHROWS:0][MESHCOLUMNS-1:0][TILECOLUMNS-1:0];
    reg signed inter_propagate[MESHROWS:0][MESHCOLUMNS-1:0][TILECOLUMNS-1:0];
    reg signed inter_valid[MESHROWS:0][MESHCOLUMNS-1:0][TILECOLUMNS-1:0];

    // the first row (for north inputs) or col (for west inputs) is automatically assigned to be the input
    // the last row (for north inputs that are returned) is automatically assigned to be the output
    integer i, j, t;
    for (i = 0; i < MESHROWS; i++) begin
        for (t = 0; t < TILEROWS; t++) begin
            assign inter_a[i][0][t] = in_a[i][t];
        end
    end
    for (j = 0; j < MESHCOLUMNS; j++) begin
        for (t = 0; t < TILECOLUMNS; t++) begin
            assign inter_b[0][j][t] = in_b[j][t];
            assign inter_d[0][j][t] = in_d[j][t];
            assign inter_dataflow[0][j][t] = in_dataflow[j][t];
            assign inter_propagate[0][j][t] = in_propagate[j][t];
            assign inter_valid[0][j][t] = in_valid[j][t];

            assign out_b[j][t] = inter_b[j][MESHROWS][t];
            assign out_c[j] = inter_d[j][MESHROWS][t];
            assign out_valid[j] = inter_valid[j][MESHROWS][t];
        end
    end

    // generate Tile blocks and connect via intermediate registers
    // (connect edge Tiles blocks to inputs/outputs)
    genvar i, j;
    generate
        for (i = 0; i < MESHROWS; i++) begin
            for (j = 0; j < MESHCOLUMNS; j++) begin
                Tile tile_instance #(BITWIDTH, TILEROWS, TILECOLUMNS) (
                    .in_a(inter_a[i][j]),
                    .in_b(inter_b[i][j]),
                    .in_d(inter_d[i][j]),
                    .in_dataflow(inter_dataflow[i][j]),
                    .in_propagate(inter_propagate[i][j]),
                    .in_valid(inter_valid[i][j]),
                    .out_a(inter_a[i][j + 1])
                    .out_b(inter_b[i + 1][j]),
                    .out_c(inter_c[i + 1][j]),
                    .out_dataflow(inter_dataflow[i + 1][j]),
                    .out_propagate(inter_propagate[i + 1][j]),
                    .out_valid(inter_valid[i + 1][j])
                );

                always @(posedge clock) begin
                    inter_a[i][j + 1] <= tile_instance.out_a;
                    inter_b[i + 1][j] <= tile_instance.out_b;
                    inter_c[i + 1][j] <= tile_instance.out_c;
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
        input signed [BITWIDTH-1:0] in_a[TILEROWS-1:0];
        input signed [BITWIDTH-1:0] in_b[TILECOLUMNS-1:0];
        input signed [BITWIDTH-1:0] in_d[TILECOLUMNS-1:0];
        input in_dataflow[TILECOLUMNS-1:0];
        input in_propagate[TILECOLUMNS-1:0];
        input in_valid[TILECOLUMNS-1:0];
        output signed [BITWIDTH-1:0] out_a[TILECOLUMNS-1:0];
        output signed [BITWIDTH-1:0] out_b[TILECOLUMNS-1:0];
        output signed [BITWIDTH-1:0] out_c[TILECOLUMNS-1:0];
        output out_dataflow[TILECOLUMNS-1:0];
        output out_propagate[TILECOLUMNS-1:0];
        output out_valid[TILECOLUMNS-1:0];
    );

    // store intermediate outputs in wires
    wire signed [BITWIDTH-1:0] inter_a[TILEROWS-1:0][TILECOLUMNS:0];
    wire signed [BITWIDTH-1:0] inter_b[TILEROWS:0][TILECOLUMNS-1:0];
    wire signed [BITWIDTH-1:0] inter_d[TILEROWS:0][TILECOLUMNS-1:0];
    wire signed inter_dataflow[TILEROWS:0][TILECOLUMNS-1:0];
    wire signed inter_propagate[TILEROWS:0][TILECOLUMNS-1:0];
    wire signed inter_valid[TILEROWS:0][TILECOLUMNS-1:0];

    // the first row (for north inputs) or col (for west inputs) is automatically assigned to be the input
    // the last row (for north inputs) or col (for west inputs) is automatically assigned to be the output
    integer i, j;
    for (i = 0; i < TILEROWS; i++) begin
        assign inter_a[i][0] = in_a[i];
        assign out_a[i] = inter_a[i][TILECOLUMNS];
    end
    for (j = 0; j < TILECOLUMNS; j++) begin
        assign inter_b[0][j] = in_b[j];
        assign inter_d[0][j] = in_d[j];
        assign inter_dataflow[0][j] = in_dataflow[j];
        assign inter_propagate[0][j] = in_propagate[j];
        assign inter_valid[0][j] = in_valid[j];

        assign out_b[j] = inter_b[j][TILEROWS];
        assign out_d[j] = inter_d[j][TILEROWS];
        assign out_dataflow[j] = inter_dataflow[j][TILEROWS];
        assign out_propagate[j] = inter_propagate[j][TILEROWS];
        assign out_valid[j] = inter_valid[j][TILEROWS];
    end

    // generate PE blocks and connect via intermediate wires
    // (connect edge PE blocks to inputs/outputs)
    genvar i, j;
    generate
        for (i = 0; i < TILEROWS; i++) begin
            for (j = 0; j < TILECOLUMNS; j++) begin
                PE pe_instance #(BITWIDTH) (
                    .in_a(inter_a[i][j]),
                    .in_b(inter_b[i][j]),
                    .in_d(inter_d[i][j]),
                    .in_dataflow(inter_dataflow[i][j]),
                    .in_propagate(inter_propagate[i][j]),
                    .in_valid(inter_valid[i][j]),
                    .out_a(inter_a[i][j + 1])
                    .out_b(inter_b[i + 1][j]),
                    .out_c(inter_c[i + 1][j]),
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
        input signed [BITWIDTH-1:0] in_a;
        input signed [BITWIDTH-1:0] in_b;
        input signed [BITWIDTH-1:0] in_d;
        input in_dataflow;
        input in_propagate;
        input in_valid;
        output signed [BITWIDTH-1:0] out_a;
        output signed [BITWIDTH-1:0] out_b;
        output signed [BITWIDTH-1:0] out_c;
        output out_dataflow;
        output out_propagate;
        output out_valid;
    );

endmodule

