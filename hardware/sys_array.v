module sys_array
    #(parameter MESHROWS, MESHCOLS, BITWIDTH, TILEROWS=1, TILECOLS=1)
    (
        input clock,
        input reset,
        input in_dataflow,
        input signed [BITWIDTH-1:0] in_a[MESHROWS-1:0][TILEROWS-1:0],
        input in_a_valid[MESHROWS-1:0][TILEROWS-1:0],
        input signed [BITWIDTH-1:0] in_b[MESHCOLS-1:0][TILECOLS-1:0],
        input signed [BITWIDTH-1:0] in_d[MESHCOLS-1:0][TILECOLS-1:0],
        input in_propagate[MESHCOLS-1:0][TILECOLS-1:0],
        input [BITWIDTH-1:0] in_b_shelf_life[MESHCOLS-1:0][TILECOLS-1:0],
        input in_b_valid[MESHCOLS-1:0][TILECOLS-1:0],
        input in_d_valid[MESHCOLS-1:0][TILECOLS-1:0],
        output signed [BITWIDTH-1:0] out_c[MESHCOLS-1:0][TILECOLS-1:0],
        output out_c_valid[MESHCOLS-1:0][TILECOLS-1:0]
    );

    // store intermediate outputs in wires
    reg signed [BITWIDTH-1:0] inter_a[MESHROWS-1:0][MESHCOLS:0][TILEROWS-1:0] /*verilator split_var*/;
    reg inter_a_valid[MESHROWS-1:0][MESHCOLS:0][TILEROWS-1:0] /*verilator split_var*/;
    reg signed [BITWIDTH-1:0] inter_b[MESHROWS:0][MESHCOLS-1:0][TILECOLS-1:0] /*verilator split_var*/;
    reg signed [BITWIDTH-1:0] inter_d[MESHROWS:0][MESHCOLS-1:0][TILECOLS-1:0] /*verilator split_var*/;
    reg signed inter_propagate[MESHROWS:0][MESHCOLS-1:0][TILECOLS-1:0] /*verilator split_var*/;
    reg [BITWIDTH-1:0] inter_b_shelf_life[MESHROWS:0][MESHCOLS-1:0][TILECOLS-1:0] /* verilator split_var*/;
    reg inter_b_valid[MESHROWS:0][MESHCOLS-1:0][TILECOLS-1:0] /*verilator split_var*/;
    reg inter_d_valid[MESHROWS:0][MESHCOLS-1:0][TILECOLS-1:0] /*verilator split_var*/;

    // the first row (for north inputs) or col (for west inputs) is automatically assigned to be the input
    // the last row (for north inputs that are returned) is automatically assigned to be the output
    genvar k, l, t;
    generate
        for (k = 0; k < MESHROWS; k++) begin
            for (t = 0; t < TILEROWS; t++) begin
                assign inter_a[k][0][t] = in_a[k][t];
                assign inter_a_valid[k][0][t] = in_a_valid[k][t];
            end
        end
        for (l = 0; l < MESHCOLS; l++) begin
            for (t = 0; t < TILECOLS; t++) begin
                assign inter_b[0][l][t] = in_b[l][t];
                assign inter_d[0][l][t] = in_d[l][t];
                assign inter_propagate[0][l][t] = in_propagate[l][t];
                assign inter_b_shelf_life[0][l][t] = in_b_shelf_life[l][t];
                assign inter_b_valid[0][l][t] = in_b_valid[l][t];
                assign inter_d_valid[0][l][t] = in_d_valid[l][t];

                assign out_c[l][t] = inter_d[MESHROWS][l][t];
                assign out_c_valid[l][t] = inter_d_valid[MESHROWS][l][t];
            end
        end
    endgenerate

    // generate Tile blocks and connect via intermediate registers
    // (connect edge Tiles blocks to inputs/outputs)
    genvar i, j;
    generate
        for (i = 0; i < MESHROWS; i++) begin
            for (j = 0; j < MESHCOLS; j++) begin
                Tile #(MESHROWS, MESHCOLS, BITWIDTH, TILEROWS, TILECOLS) 
                tile_instance (
                    .clock(clock),
                    .reset(reset),
                    .in_dataflow(in_dataflow),
                    .in_a(inter_a[i][j]),
                    .in_a_valid(inter_a_valid[i][j]),
                    .in_b(inter_b[i][j]),
                    .in_d(inter_d[i][j]),
                    .in_propagate(inter_propagate[i][j]),
                    .in_b_shelf_life(inter_b_shelf_life[i][j]),
                    .in_b_valid(inter_b_valid[i][j]),
                    .in_d_valid(inter_d_valid[i][j]),
                    .out_a(inter_a[i][j + 1]),
                    .out_a_valid(inter_a_valid[i][j + 1]),
                    .out_b(inter_b[i + 1][j]),
                    .out_d(inter_d[i + 1][j]),
                    .out_propagate(inter_propagate[i + 1][j]),
                    .out_b_shelf_life(inter_b_shelf_life[i + 1][j]),
                    .out_b_valid(inter_b_valid[i + 1][j]),
                    .out_d_valid(inter_d_valid[i + 1][j])
                );
            end
        end
    endgenerate
endmodule

module Tile
    #(parameter MESHROWS, MESHCOLS, BITWIDTH, TILEROWS=1, TILECOLS=1)
    (
        input clock,
        input reset,
        input in_dataflow,
        input signed [BITWIDTH-1:0] in_a[TILEROWS-1:0],
        input in_a_valid[TILEROWS-1:0],
        input signed [BITWIDTH-1:0] in_b[TILECOLS-1:0],
        input signed [BITWIDTH-1:0] in_d[TILECOLS-1:0],
        input in_propagate[TILECOLS-1:0],
        input [BITWIDTH-1:0] in_b_shelf_life[TILECOLS-1:0],
        input in_b_valid[TILECOLS-1:0],
        input in_d_valid[TILECOLS-1:0],
        output reg signed [BITWIDTH-1:0] out_a[TILECOLS-1:0],
        output reg out_a_valid[TILECOLS-1:0],
        output reg signed [BITWIDTH-1:0] out_b[TILECOLS-1:0],
        output reg signed [BITWIDTH-1:0] out_d[TILECOLS-1:0],
        output reg out_propagate[TILECOLS-1:0],
        output reg [BITWIDTH-1:0] out_b_shelf_life[TILECOLS-1:0],
        output reg out_b_valid[TILECOLS-1:0],
        output reg out_d_valid[TILECOLS-1:0]
    );

    // store intermediate outputs in wires
    wire signed [BITWIDTH-1:0] inter_a[TILEROWS-1:0][TILECOLS:0] /*verilator split_var*/;
    wire inter_a_valid[TILEROWS-1:0][TILECOLS:0] /*verilator split_var*/;
    wire signed [BITWIDTH-1:0] inter_b[TILEROWS:0][TILECOLS-1:0] /*verilator split_var*/;
    wire signed [BITWIDTH-1:0] inter_d[TILEROWS:0][TILECOLS-1:0] /*verilator split_var*/;
    wire signed inter_propagate[TILEROWS:0][TILECOLS-1:0] /*verilator split_var*/;
    wire [BITWIDTH-1:0] inter_b_shelf_life[TILEROWS:0][TILECOLS-1:0] /*verilator split_var*/;
    wire inter_b_valid[TILEROWS:0][TILECOLS-1:0] /*verilator split_var*/;
    wire inter_d_valid[TILEROWS:0][TILECOLS-1:0] /*verilator split_var*/;

    // the first row (for north inputs) or col (for west inputs) is automatically assigned to be the input
    // the last row (for north inputs) or col (for west inputs) is automatically assigned to be the output
    // ** note that the tile output is clocked (unlike for the PE output)
    genvar k, l;
    generate
        for (k = 0; k < TILEROWS; k++) begin
            assign inter_a[k][0] = in_a[k];
            assign inter_a_valid[k][0] = in_a_valid[k];

            always @(posedge clock) begin
                out_a[k] <= inter_a[k][TILECOLS];
                out_a_valid[k] <= inter_a_valid[k][TILECOLS];
            end
        end
    endgenerate
    for (l = 0; l < TILECOLS; l++) begin
        assign inter_b[0][l] = in_b[l];
        assign inter_d[0][l] = in_d[l];
        assign inter_propagate[0][l] = in_propagate[l];
        assign inter_b_shelf_life[0][l] = in_b_shelf_life[l];
        assign inter_b_valid[0][l] = in_b_valid[l];
        assign inter_d_valid[0][l] = in_d_valid[l];

        always @(posedge clock) begin
            out_b[l] <= inter_b[TILEROWS][l];
            out_d[l] <= inter_d[TILEROWS][l];
            out_propagate[l] <= inter_propagate[TILEROWS][l];
            out_b_shelf_life[l] <= inter_b_shelf_life[TILEROWS][l];
            out_b_valid[l] <= inter_b_valid[TILEROWS][l];
            out_d_valid[l] <= inter_d_valid[TILEROWS][l];
        end
    end

    // generate PE blocks and connect via intermediate wires
    // (connect edge PE blocks to inputs/outputs)
    genvar i, j;
    generate
        for (i = 0; i < TILEROWS; i++) begin
            for (j = 0; j < TILECOLS; j++) begin
                PE #(MESHROWS, MESHCOLS, BITWIDTH, TILEROWS, TILECOLS) 
                pe_instance (
                    .clock(clock),
                    .reset(reset),
                    .in_dataflow(in_dataflow),
                    .in_a(inter_a[i][j]),
                    .in_a_valid(inter_a_valid[i][j]),
                    .in_b(inter_b[i][j]),
                    .in_d(inter_d[i][j]),
                    .in_propagate(inter_propagate[i][j]),
                    .in_b_shelf_life(inter_b_shelf_life[i][j]),
                    .in_b_valid(inter_b_valid[i][j]),
                    .in_d_valid(inter_d_valid[i][j]),
                    .out_a(inter_a[i][j + 1]),
                    .out_a_valid(inter_a_valid[i][j + 1]),
                    .out_b(inter_b[i + 1][j]),
                    .out_d(inter_d[i + 1][j]),
                    .out_propagate(inter_propagate[i + 1][j]),
                    .out_b_shelf_life(inter_b_shelf_life[i + 1][j]),
                    .out_b_valid(inter_b_valid[i + 1][j]),
                    .out_d_valid(inter_d_valid[i + 1][j])
                );
            end
        end
    endgenerate
endmodule


module PE
    #(parameter MESHROWS, MESHCOLS, BITWIDTH, TILEROWS=1, TILECOLS=1)
    (
        input clock,
        input reset,
        input in_dataflow,
        input signed [BITWIDTH-1:0] in_a,
        input in_a_valid,
        input signed [BITWIDTH-1:0] in_b,
        input signed [BITWIDTH-1:0] in_d,
        input in_propagate,
        input [BITWIDTH-1:0] in_b_shelf_life,
        input in_b_valid,
        input in_d_valid,
        output signed [BITWIDTH-1:0] out_a,
        output out_a_valid,
        output signed [BITWIDTH-1:0] out_b,
        output signed [BITWIDTH-1:0] out_d,
        output out_propagate,
        output [BITWIDTH-1:0] out_b_shelf_life,
        output out_b_valid,
        output out_d_valid
    );

    reg dataflow;
    reg signed [BITWIDTH-1:0] b0;
    reg signed [BITWIDTH-1:0] b1;
    reg valid0;
    reg valid1;
    reg [BITWIDTH-1:0] shelf_life0;
    reg [BITWIDTH-1:0] shelf_life1;

    always @(posedge clock) begin
        if (reset) begin
            valid0 <= 0;
            valid1 <= 0;
            dataflow <= in_dataflow;
        end
        else begin
            if (dataflow) begin
                // weight-stationary dataflow
                b0 <= (!in_b_valid || in_b_shelf_life == 0 || in_propagate) ? b0 : in_b;
                b1 <= (!in_b_valid || in_b_shelf_life == 0 || !in_propagate) ? b1 : in_b;
                valid0 <= (!in_b_valid || in_b_shelf_life == 0 || in_propagate) ? valid0 : in_b_valid;
                valid1 <= (!in_b_valid || in_b_shelf_life == 0 || !in_propagate) ? valid1 : in_b_valid;
                shelf_life0 <= (!in_b_valid || in_b_shelf_life == 0 || in_propagate) ? shelf_life0 : in_b_shelf_life;
                shelf_life1 <= (!in_b_valid || in_b_shelf_life == 0 || !in_propagate) ? shelf_life1 : in_b_shelf_life;
            end
            else begin
                // output stationary (accumulator) dataflow
                if (in_b_valid && in_b_shelf_life > 0 && !in_propagate) begin
                    b0 <= in_b;
                    valid0 <= in_b_valid;
                    shelf_life0 <= in_b_shelf_life;
                end
                else begin
                    if (in_a_valid && in_d_valid)
                        b0 <= b0 + (in_a * in_d);
                    valid0 <= valid0;
                    shelf_life0 <= shelf_life0;
                end
                if (in_b_valid && in_b_shelf_life > 0 && in_propagate) begin
                    b1 <= in_b;
                    valid1 <= in_b_valid;
                    shelf_life1 <= in_b_shelf_life;
                end
                else begin
                    if (in_a_valid && in_d_valid)
                        b1 <= b1 + (in_a * in_d);
                    valid1 <= valid1;
                    shelf_life1 <= shelf_life1;
                end
            end
        end
    end
    
    assign out_a = in_a;
    assign out_a_valid = in_a_valid;
    assign out_b = (in_propagate ? b1 : b0);
    assign out_d = dataflow ? (in_d + in_a * (in_propagate ? b0 : b1)) : (in_propagate ? b0 : b1);
    assign out_propagate = in_propagate;
    assign out_b_shelf_life = (in_propagate ? (shelf_life1 == 0 ? 0 : shelf_life1 - 1)
                                            : (shelf_life0 == 0 ? 0 : shelf_life0 - 1));
    assign out_b_valid = (in_propagate ? valid1 : valid0);
    assign out_d_valid = in_a_valid & in_d_valid & (in_propagate ? valid0 : valid1);

endmodule

