module sys_array_controller
    #(
        parameter BITWIDTH, ADDRWIDTH, MESHUNITS, TILEUNITS
    )
    (
        input clock,
        input reset,

        // CONTROL SIGNALS
        input comp_lock_req [1:0],
        input [ADDRWIDTH-1:0] A_addr [1:0],
        input [ADDRWIDTH-1:0] D_addr [1:0],
        input [ADDRWIDTH-1:0] C_addr [1:0],
        output comp_lock_res [1:0], // will never have "1"s overlap with load_lock_res
        output comp_finished,

        input load_lock_req [1:0],
        input [ADDRWIDTH-1:0] b_addr [1:0],
        output load_lock_res [1:0], // will never have "1"s overlap with comp_lock_res
        output load_finished,

        // MEMORY READ SIGNALS
        input signed [BITWIDTH-1:0] A [MESHUNITS-1:0][TILEUNITS-1:0],
        input signed [BITWIDTH-1:0] D [MESHUNITS-1:0][TILEUNITS-1:0],
        input signed [BITWIDTH-1:0] B [MESHUNITS-1:0][TILEUNITS-1:0],
        output [ADDRWIDTH-1:0] A_row_read_addrs [MESHUNITS-1:0],
        output [ADDRWIDTH-1:0] D_col_read_addrs [MESHUNITS-1:0],
        output [ADDRWIDTH-1:0] B_col_read_addrs [MESHUNITS-1:0],

        // MEMORY WRITE SIGNALS
        output [BITWIDTH-1:0] C [MESHUNITS-1:0][TILEUNITS-1:0],
        output [ADDRWIDTH-1:0] C_col_write_addrs [MESHUNITS-1:0],
        output C_write_valid [MESHUNITS-1]
    );

    localparam 
        LOCK_FREE = 2'b00,
        LOCK_ZERO = 2'b01,
        LOCK_ONE = 2'b10;

    reg [1:0] comp_lock;
    reg [31:0] comp_tick_ctr;
    reg comp_complete;
    reg [ADDRWIDTH-1:0] A_base_addr;
    reg [ADDRWIDTH-1:0] D_base_addr;
    reg [ADDRWIDTH-1:0] C_base_addr;
    wire A_valid [MESHUNITS=1:0][TILEUNITS-1:0];
    wire D_valid [MESHUNITS=1:0][TILEUNITS-1:0];

    reg [1:0] load_lock;
    reg [31:0] load_tick_ctr;
    reg load_complete;
    reg [ADDRWIDTH-1:0] B_base_addr;

    always @(*) begin
        if (comp_lock == LOCK_FREE) begin
            A_valid = 0;
            D_valid = 0;
            A_row_read_addrs = 0;
            D_row_read_addrs = 0;
        end
        else begin
            // READ ADDRS
            genvar i, j;
            for (i = 0; i < MESHUNITS; i++) begin
                if (comp_tick_ctr >= i && comp_tick_ctr < (MESHUNITS * TILEUNITS) + i) begin
                    A_row_read_addrs[i] = A_base_addr + (comp_tick_ctr - i) * MESHUNITS * TILEUNITS + i * TILEUNITS;
                    D_row_read_addrs[i] = D_base_addr + (comp_tick_ctr - i) * MESHUNITS * TILEUNITS + i * TILEUNITS;
                end
                else begin
                    A_row_read_addrs[i] = 0;
                    D_row_read_addrs[i] = 0;
                end
            end

            // READ VALID (TO SYS ARRAY)
            if (comp_tick_ctr == 0) begin
                A_valid = 0;
                D_valid = 0;
            end
            else begin
                wire [31:0] prev_comp_tick_ctr = comp_tick_ctr - 1;
                for (i = 0; i < MESHUNITS; i++) begin
                    for (j = 0; j < TILEUNITS; j++) begin
                        if (prev_comp_tick_ctr >= i && prev_comp_tick_ctr < (MESHUNITS * TILEUNITS) + i) begin
                            A_valid[i][j] = 1;
                            D_valid[i][j] = 1;
                        end
                        else begin
                            A_valid[i][j] = 0;
                            D_valid[i][j] = 0;
                        end
                    end
                end
            end
        end
    end

    always @(posedge clock) begin
        if (reset) begin
            comp_lock <= LOCK_FREE;
            comp_complete <= 0;
            load_lock <= LOCK_FREE;
            load_complete <= 0;
        end
        else begin

            // SYNCHRONIZATION UPDATE
            if (comp_complete) begin
                comp_complete <= 0;
                comp_lock <= LOCK_FREE;
            end
            if (load_complete) begin
                load_complete <= 0;
                load_lock <= LOCK_FREE;
            end
            if (comp_lock == LOCK_FREE && load_lock == LOCK_FREE) begin
                if (comp_lock_req[0]) begin
                    // assign comp_lock to 0 and try to assign load_lock to 1
                    comp_lock <= LOCK_ZERO;
                    comp_tick_ctr <= 0;
                    A_base_addr <= A_addr[0];
                    D_base_addr <= D_addr[0];
                    C_base_addr <= C_addr[0];
                    if (load_lock_req[1]) begin
                        load_lock <= LOCK_ONE;
                        B_base_addr <= B_addr[1];
                    end
                    else begin
                        load_lock <= LOCK_FREE;
                    end
                end
                else if (comp_lock_req[1]) begin
                    // assign comp_lock to 1 and try to assign load lock to 0
                    comp_lock <= LOCK_ONE;
                    comp_tick_ctr <= 0;
                    A_base_addr <= A_addr[1];
                    D_base_addr <= D_addr[1];
                    C_base_addr <= C_addr[1];
                    if (load_lock_req[0]) begin
                        load_lock <= LOCK_ZERO;
                        B_base_addr <= B_addr[0];
                    end
                    else begin
                        load_lock <= LOCK_FREE;
                    end
                end
                else begin
                    // assign load lock to lowest index requester
                    comp_lock <= LOCK_FREE;
                    if (load_lock_req[0]) begin
                        load_lock <= LOCK_ZERO;
                        B_base_addr <= B_addr[0];
                    end
                    else if (load_lock_req[1]) begin
                        load_lock <= LOCK_ONE;
                        B_base_addr <= B_addr[1];
                    end
                    else begin
                        load_lock <= LOCK_FREE;
                    end
                end
            end
            else if (comp_lock == LOCK_FREE) begin
                // try to assign comp lock to first thread that doesn't hold load lock
                if (load_lock != LOCK_ZERO && comp_lock_req[0]) begin
                    comp_lock <= LOCK_ZERO;
                    comp_tick_ctr <= 0;
                    A_base_addr <= A_addr[0];
                    D_base_addr <= D_addr[0];
                    C_base_addr <= C_addr[0];
                end
                else if (load_lock != LOCK_ONE && comp_lock_req[1]) begin
                    comp_lock <= LOCK_ONE;
                    comp_tick_ctr <= 0;
                    A_base_addr <= A_addr[1];
                    D_base_addr <= D_addr[1];
                    C_base_addr <= C_addr[1];
                end
            end
            else if (load_lock == LOCK_FREE) begin
                // try to assign load lock to first thread that doesn't hold comp lock
                if (comp_lock != LOCK_ZERO && load_lock_req[0]) begin
                    load_lock <= LOCK_ZERO;
                    B_base_addr <= B_addr[0];
                end
                else if (comp_lock != LOCK_ONE && load_lock_req[1]) begin
                    load_lock <= LOCK_ONE;
                    B_base_addr <= B_addr[1];
                end
            end
        end
    end


    sys_array #(parameter MESHROWS=MESHUNITS, MESHCOLS=MESHUNITS, BITWIDTH=BITWIDTH, TILEROWS=TILEUNITS, TILECOLS=TILEUNITS)
    sys_array_module (
        .clock(clock),
        .reset(reset),
        .in_dataflow(),
        .in_a(A),
        .in_a_valid(A_valid),
        .in_b(),
        .in_d(D),
        .in_propagate(),
        .in_b_shelf_life(),
        .in_b_valid(),
        .in_d_valid(D_valid),
        .out_c(),
        .out_c_valid()
    );


endmodule