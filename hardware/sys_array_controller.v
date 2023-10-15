module sys_array_controller
    #(
        parameter BITWIDTH, ADDRWIDTH,
            MESHROWS, MESHCOLS, BITWIDTH, TILEROWS, TILECOLS
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
        input [BITWIDTH-1:0] A [MESHROWS-1:0][TILEROWS-1:0],
        input [BITWIDTH-1:0] D [MESHCOLS-1:0][TILECOLS-1:0],
        input [BITWIDTH-1:0] B [MESHCOLS-1:0][TILECOLS-1:0],
        output [ADDRWIDTH-1:0] A_row_read_addrs [MESHROWS-1:0],
        output [ADDRWIDTH-1:0] D_col_read_addrs [MESHCOLS-1:0],
        output [ADDRWIDTH-1:0] B_col_read_addrs [MESHCOLS-1:0],

        // MEMORY WRITE SIGNALS
        output [BITWIDTH-1:0] C [MESHCOLS-1:0][TILECOLS-1:0],
        output [ADDRWIDTH-1:0] C_col_write_addrs [MESHCOLS-1:0],
        output C_write_valid [MESHCOLS-1]
    );

    localparam 
        LOCK_FREE = 2'b00,
        LOCK_ZERO = 2'b01,
        LOCK_ONE = 2'b10;

    reg [1:0] comp_lock;
    reg [ADDRWIDTH-1:0] A_base_addr;
    reg [ADDRWIDTH-1:0] D_base_addr;
    reg [ADDRWIDTH-1:0] C_base_addr;

    reg [1:0] load_lock;
    reg [ADDRWIDTH-1:0] B_base_addr;

    always @(posedge clock) begin
        if (reset) begin
            comp_lock <= LOCK_FREE;
            load_lock <= LOCK_FREE;
        end
        else begin
            if (comp_lock == LOCK_FREE && load_lock == LOCK_FREE) begin
                if (comp_lock_req[0]) begin
                    // assign comp_lock to 0 and try to assign load_lock to 1
                    comp_lock <= LOCK_ZERO;
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
                    A_base_addr <= A_addr[0];
                    D_base_addr <= D_addr[0];
                    C_base_addr <= C_addr[0];
                end
                else if (load_lock != LOCK_ONE && comp_lock_req[1]) begin
                    comp_lock <= LOCK_ONE;
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