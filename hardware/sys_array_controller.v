module sys_array_controller
    #(
        parameter BITWIDTH, ADDRWIDTH, MESHUNITS, TILEUNITS
    )
    (
        input clock,
        input reset,

        // COMP CONTROL SIGNALS
        input comp_lock_req [1:0],
        input [ADDRWIDTH-1:0] A_addr [1:0],
        input [ADDRWIDTH-1:0] D_addr [1:0],
        input [ADDRWIDTH-1:0] C_addr [1:0],
        output comp_lock_res [1:0], // will never have "1"s overlap with load_lock_res
        output comp_finished,

        // LOAD CONTROL SIGNALS
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

    // COMP STATE
    reg [1:0] comp_lock;
    reg [31:0] comp_tick_ctr;
    reg comp_complete;
    reg [ADDRWIDTH-1:0] A_base_addr;
    reg [ADDRWIDTH-1:0] D_base_addr;
    wire A_valid [MESHUNITS-1:0][TILEUNITS-1:0];
    wire D_valid [MESHUNITS-1:0][TILEUNITS-1:0];
    reg [ADDRWIDTH-1:0] C_base_addr;
    wire [BITWIDTH-1:0] C_buffer [MESHUNITS-1:0][TILEUNITS-1:0];
    wire C_valid_buffer [MESHUNITS-1:0][TILEUNITS-1:0];

    // LOAD STATE
    reg [1:0] load_lock;
    reg [31:0] load_tick_ctr;
    reg load_complete;
    reg [ADDRWIDTH-1:0] B_base_addr;
    wire B_valid [MESHUNITS-1:0][TILEUNITS-1:0];
    wire [$clog2(MESHUNITS * TILEUNITS):0] B_shelf_life [MESHUNITS-1:0][TILEUNITS-1:0];
    wire B_propagate [MESHUNITS-1:0][TILEUNITS-1:0];

    always @(*) begin

        //
        // COMP LOGIC
        //

        if (comp_lock == LOCK_FREE) begin
            A_row_read_addrs = 0;
            D_row_read_addrs = 0;
            A_valid = 0;
            D_valid = 0;
            C = 0;
            C_col_write_addrs = 0;
            C_write_valid = 0;
        end
        else begin
            // READ COMP ADDRS (A, D)
            genvar i, j;
            for (i = 0; i < MESHUNITS; i++) begin
                // row/col i of the sys array receives an input signal
                // iff k <= i < k + (MU * TU)
                // i.e., row/col i starts reading MU * TU values at counter k = i
                if (comp_tick_ctr >= i && comp_tick_ctr < (MESHUNITS * TILEUNITS) + i) begin
                    // at counter k read from addr:
                    // A/D[k - i][i] = A/D_base_addr + (k - i) * (MU * TU) + i * (TU)
                    A_row_read_addrs[i] = A_base_addr + (comp_tick_ctr - i) * MESHUNITS * TILEUNITS + i * TILEUNITS;
                    D_row_read_addrs[i] = D_base_addr + (comp_tick_ctr - i) * MESHUNITS * TILEUNITS + i * TILEUNITS;
                end
                else begin
                    A_row_read_addrs[i] = 0;
                    D_row_read_addrs[i] = 0;
                end
            end

            // READ COMP VALID (TO SYS ARRAY)
            if (comp_tick_ctr == 0) begin
                A_valid = 0;
                D_valid = 0;
            end
            else begin
                // determine validity of input signal from previous rules applied to 
                // counter - 1 (since we read from mem on the following cycle)
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

            // WRITE ADDR (C)
            for (i = 0; i < MESHUNITS; i++) begin
                // col i of the sys array produces a valid output signal
                // iff (k - (MU + 1)) <= i < (k - (MU + 1)) + (MU * TU)
                // i.e., col i start writing MU * TU values at counter k = i + (MU + 1)
                // note that this is MU + 1 greater than the read signal start 
                // (1 cycle to read input from memory + MU cycles to propagate)
                if (comp_tick_ctr >= (MESHUNITS + 1 + i) && comp_tick_ctr < ((MESHUNITS + 1 + i) + (MESHUNITS * TILEUNITS))) begin
                    C_col_write_addrs[i] = C_base_addr + (comp_tick_ctr - (MESHUNITS + 1 + i)) * MESHUNITS * TILEUNITS + i * TILEUNITS;
                    C_write_valid[i] = C_valid_buffer[i];
                end
                else begin
                    C_col_write_addrs[i] = 0;
                    C_write_valid = 0;
                end
            end
            C = C_buffer;
        end

        // COMP COMPLETE on the cycle after the final C write goes through
        // --> k = ((MU + 1 + i) + (MU * TU)), i = final col (MU - 1)
        // --> k = ((MU + 1 + MU - 1)) + (MU * TU)
        // --> k = MU * (2 + TU)
        comp_complete = comp_tick_ctr == MESHUNITS * (2 + TILEUNITS)

        //
        // LOAD LOGIC
        //

        if (load_lock == LOCK_FREE) begin
            B_row_read_addrs = 0;
            B_valid = 0;
            B_shelf_life = 0;
            B_propagate = 0;
        end
        else begin
            // READ LOAD ADDR (B)
            for (i = 0; i < MESHUNITS; i++) begin
                // col i of the sys array receives an input signal
                // iff l <= i < l + (MU * TU)
                // i.e., col i starts reading MU * TU values at counter l = i
                if (load_tick_ctr >= i && load_tick_ctr < (MESHUNITS * TILEUNITS) + i) begin
                    // at counter l read from addr:
                    // B[k - i][i] = B_base_addr + (k - i) * (MU * TU) + i * (TU)
                    B_row_read_addrs[i] = B_base_addr + (load_tick_ctr - i) * MESHUNITS * TILEUNITS + i * TILEUNITS;
                end
                else begin
                    B_row_read_addrs[i] = 0;
                end
            end

            // READ LOAD VALID + SHELF LIFE + PROPAGATE (TO SYS ARRAY)
            if (load_tick_ctr == 0) begin
                B_valid = 0;
                B_shelf_life = 0;
                B_propagate = 0;
            end
            else begin
                // determine validity of input signal from previous rules applied to 
                // counter - 1 (since we read from mem on the following cycle)
                wire [31:0] prev_load_tick_ctr = load_tick_ctr - 1;
                for (i = 0; i < MESHUNITS; i++) begin
                    for (j = 0; j < TILEUNITS; j++) begin
                        if (prev_load_tick_ctr >= i && prev_load_tick_ctr < (MESHUNITS * TILEUNITS) + i) begin
                            B_valid[i][j] = 1;
                            B_shelf_life[i][j] = (MESHUNITS * TILEUNITS) - (prev_load_tick_ctr - i);
                            B_propagate[i][j] = load_lock == LOCK_ZERO ? 0 : 1;
                        end
                        else begin
                            B_valid[i][j] = 0;
                            B_shelf_life[i][j] = 0;
                            B_propagate[i][j] = 0;
                        end
                    end
                end
            end            
        end
        // LOAD COMPLETE on the cycle after last col/last row loaded into sys array
        // --> l - 1 = MU * TU + i, i = final col (MU - 1)
        // --> l = MU * TU + MU - 1 + 1
        // --> l = MU * (1 + TU)
        load_complete = load_tick_ctr == MESHUNITS * (1 + TILEUNITS)
    end

    always @(posedge clock) begin
        if (reset) begin
            comp_lock <= LOCK_FREE;
            comp_complete <= 0;
            load_lock <= LOCK_FREE;
            load_complete <= 0;
        end
        else begin

            //
            // SYNCHRONIZATION LOGIC
            //

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
    assign comp_lock_res = comp_lock;
    assign load_lock_res = load_lock;

    //
    // INTERNAL SYS ARRAY MODULE
    //
    
    sys_array #(parameter MESHROWS=MESHUNITS, MESHCOLS=MESHUNITS, BITWIDTH=BITWIDTH, TILEROWS=TILEUNITS, TILECOLS=TILEUNITS)
    sys_array_module (
        .clock(clock),
        .reset(reset),
        .in_dataflow(1),
        .in_a(A),
        .in_a_valid(A_valid),
        .in_b(B),
        .in_d(D),
        .in_propagate(B_propagate),
        .in_b_shelf_life(B_shelf_life),
        .in_b_valid(B_valid),
        .in_d_valid(D_valid),
        .out_c(C_buffer),
        .out_c_valid(C_valid_buffer)
    );

endmodule
