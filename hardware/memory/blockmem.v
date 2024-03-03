module blockmem
    #(
        parameter ADDRSIZE, BITWIDTH, MESHUNITS, TILEUNITS
    )
    (
        input clock,
        input reset,

        // MEMORY READ SIGNALS
        // array
        input [BITWIDTH-1:0] A_tile_read_addrs [MESHUNITS-1:0],
        input [BITWIDTH-1:0] D_tile_read_addrs [MESHUNITS-1:0],
        input [BITWIDTH-1:0] B_tile_read_addrs [MESHUNITS-1:0],
        input A_read_valid [MESHUNITS-1:0], // UNUSED (for testing)
        input D_read_valid [MESHUNITS-1:0], // UNUSED (for testing)
        input B_read_valid [MESHUNITS-1:0], // UNUSED (for testing)
        output signed [BITWIDTH-1:0] A [MESHUNITS-1:0][TILEUNITS-1:0],
        output signed [BITWIDTH-1:0] D [MESHUNITS-1:0][TILEUNITS-1:0],
        output signed [BITWIDTH-1:0] B [MESHUNITS-1:0][TILEUNITS-1:0],

        // thread
        input [BITWIDTH-1:0] thread0_read_addr,
        output signed [BITWIDTH-1:0] thread0_read_data [(MESHUNITS * MESHUNITS * TILEUNITS * TILEUNITS) - 1:0],

        // MEMORY WRITE SIGNALS
        // array
        input [BITWIDTH-1:0] C_tile_write_addrs [MESHUNITS-1:0],
        input C_write_valid [MESHUNITS-1:0],
        input [BITWIDTH-1:0] C [MESHUNITS-1:0][TILEUNITS-1:0],
        
        // loader
        input [BITWIDTH-1:0] loader_write_addr,
        input loader_write_valid,
        input [BITWIDTH-1:0] loader_write_data [(MESHUNITS * MESHUNITS * TILEUNITS * TILEUNITS) - 1:0]
    );

    // MEMORY
    reg signed [BITWIDTH-1:0] block_mem [ADDRSIZE-1:0] /*verilator public*/;

    // STATE + PARAMS
    // array
    reg signed [BITWIDTH-1:0] A_buffer [MESHUNITS-1:0][TILEUNITS-1:0];
    reg signed [BITWIDTH-1:0] D_buffer [MESHUNITS-1:0][TILEUNITS-1:0];
    reg signed [BITWIDTH-1:0] B_buffer [MESHUNITS-1:0][TILEUNITS-1:0];
    assign A = A_buffer;
    assign D = D_buffer;
    assign B = B_buffer;

    // thread
    reg signed [BITWIDTH-1:0] thread0_buffer [BLOCK_SIZE - 1:0];
    assign thread0_read_data = thread0_buffer;

    // loader
    localparam BLOCK_SIZE = MESHUNITS * MESHUNITS * TILEUNITS * TILEUNITS;

    always @(*) begin
        integer i, j, k;

        // array addrs + reads
        for (i = 0; i < MESHUNITS; i++) begin
            for (j = 0; j < TILEUNITS; j++) begin
                A_buffer[i][j] = block_mem[((A_tile_read_addrs[i] >> $clog2(TILEUNITS)) << $clog2(TILEUNITS)) + j];
                D_buffer[i][j] = block_mem[((D_tile_read_addrs[i] >> $clog2(TILEUNITS)) << $clog2(TILEUNITS)) + j];
                B_buffer[i][j] = block_mem[((B_tile_read_addrs[i] >> $clog2(TILEUNITS)) << $clog2(TILEUNITS)) + j];
            end
        end

        // thread reads
        for (k = 0; k < BLOCK_SIZE; k++) begin
            thread0_buffer[k] = block_mem[((thread0_read_addr >> $clog2(TILEUNITS)) << $clog2(TILEUNITS)) + k];
        end
    end


    always @(posedge clock) begin
        if (reset) begin
            block_mem <= '{default: '0};
        end
        else begin
            // 2. array writes
            integer i, j;
            for (i = 0; i < MESHUNITS; i++) begin
                if (C_write_valid[i]) begin
                    for (j = 0; j < TILEUNITS; j++) begin
                        block_mem[(((C_tile_write_addrs[i] >> $clog2(TILEUNITS)) << $clog2(TILEUNITS)) & (ADDRSIZE - 1)) + j] <= C[i][j];
                    end
                end
            end

            // 1. loader writes
            if (loader_write_valid) begin
                for (i = 0; i < BLOCK_SIZE; i++) begin
                    block_mem[(((loader_write_addr >> $clog2(BLOCK_SIZE)) << $clog2(BLOCK_SIZE)) & (ADDRSIZE - 1)) + i] = loader_write_data[i];
                end
            end
        end
    end
endmodule
