module blockmem
    #(
        parameter ADDRSIZE, BITWIDTH, MESHUNITS, TILEUNITS
    )
    (
        input clock,
        input reset,

        // MEMORY READ SIGNALS
        input [BITWIDTH-1:0] A_tile_read_addrs [MESHUNITS-1:0],
        input [BITWIDTH-1:0] D_tile_read_addrs [MESHUNITS-1:0],
        input [BITWIDTH-1:0] B_tile_read_addrs [MESHUNITS-1:0],
        input A_read_valid [MESHUNITS-1:0], // UNUSED (for testing)
        input D_read_valid [MESHUNITS-1:0], // UNUSED (for testing)
        input B_read_valid [MESHUNITS-1:0], // UNUSED (for testing)
        output signed [BITWIDTH-1:0] A [MESHUNITS-1:0][TILEUNITS-1:0],
        output signed [BITWIDTH-1:0] D [MESHUNITS-1:0][TILEUNITS-1:0],
        output signed [BITWIDTH-1:0] B [MESHUNITS-1:0][TILEUNITS-1:0],

        // MEMORY WRITE SIGNALS
        input [BITWIDTH-1:0] C_tile_write_addrs [MESHUNITS-1:0],
        input C_write_valid [MESHUNITS-1:0],
        input [BITWIDTH-1:0] C [MESHUNITS-1:0][TILEUNITS-1:0]
    );

    // MEMORY
    reg signed [BITWIDTH-1:0] block_mem [ADDRSIZE-1:0];

    assign tile_addr_mask = {(BITWIDTH - TILEUNITS){1'b1}, TILEUNITS{1'b0}};

    reg [BITWIDTH-1:0] A_tile_addr [MESHUNITS-1:0];
    reg [BITWIDTH-1:0] D_tile_addr [MESHUNITS-1:0];
    reg [BITWIDTH-1:0] B_tile_addr [MESHUNITS-1:0];
    reg [BITWIDTH-1:0] C_tile_addr [MESHUNITS-1:0];
    reg signed [BITWIDTH-1:0] A_buffer [MESHUNITS-1:0][TILEUNITS-1:0],
    reg signed [BITWIDTH-1:0] D_buffer [MESHUNITS-1:0][TILEUNITS-1:0],
    reg signed [BITWIDTH-1:0] B_buffer [MESHUNITS-1:0][TILEUNITS-1:0],

    always @(*) begin
        integer i;
        for (i = 0; i < MESHUNITS; i++) begin
            A_tile_addr[i] = A_tile_read_addrs[i] & tile_addr_mask;
            D_tile_addr[i] = D_tile_read_addrs[i] & tile_addr_mask;
            B_tile_addr[i] = B_tile_read_addrs[i] & tile_addr_mask;
            C_tile_addr[i] = C_tile_read_addrs[i] & tile_addr_mask;

            A_buffer[i] = block_mem[A_tile_addr + TILEUNITS - 1:A_tile_addr];
            D_buffer[i] = block_mem[D_tile_addr + TILEUNITS - 1:D_tile_addr];
            B_buffer[i] = block_mem[B_tile_addr + TILEUNITS - 1:B_tile_addr];
        end
    end

    assign A = A_buffer;
    assign D = D_buffer;
    assign B = B_buffer;

    always @(posedge clock) begin
        if (reset) begin
            integer i;
            for (i = 0; i < ADDRSIZE; i++) begin
                block_mem[i] <= 0;
            end
        end
        else begin
            integer i;
            for (i = 0; i < MESHUNITS; i++) begin
                if (C_write_valid[i]) begin
                    block_mem[C_tile_addr[i] + TILEUNITS - 1:C_tile_addr[i]] <= C[i][TILEUNITS-1:0];
                end
            end
        end
    end
endmodule
