module sys_array_controller
    #(
        parameter BITWIDTH, MESHUNITS, TILEUNITS
    )
    (
        input clock,
        input reset,

        // COMP CONTROL SIGNALS
        input comp_lock_req [1:0],
        input [BITWIDTH-1:0] A_addr [1:0],
        input [BITWIDTH-1:0] D_addr [1:0],
        input [BITWIDTH-1:0] C_addr [1:0],
        output comp_lock_res [1:0], // will never have "1"s overlap with load_lock_res
        output comp_finished,

        // LOAD CONTROL SIGNALS
        input load_lock_req [1:0],
        input [BITWIDTH-1:0] B_addr [1:0],
        output load_lock_res [1:0], // will never have "1"s overlap with comp_lock_res
        output load_finished,

        // MEMORY READ SIGNALS
        input signed [BITWIDTH-1:0] A [MESHUNITS-1:0][TILEUNITS-1:0],
        input signed [BITWIDTH-1:0] D [MESHUNITS-1:0][TILEUNITS-1:0],
        input signed [BITWIDTH-1:0] B [MESHUNITS-1:0][TILEUNITS-1:0],
        output [BITWIDTH-1:0] A_row_read_addrs [MESHUNITS-1:0],
        output [BITWIDTH-1:0] D_col_read_addrs [MESHUNITS-1:0],
        output [BITWIDTH-1:0] B_col_read_addrs [MESHUNITS-1:0],
        output A_read_valid [MESHUNITS-1:0],
        output D_read_valid [MESHUNITS-1:0],
        output B_read_valid [MESHUNITS-1:0],

        // MEMORY WRITE SIGNALS
        output [BITWIDTH-1:0] C [MESHUNITS-1:0][TILEUNITS-1:0],
        output [BITWIDTH-1:0] C_col_write_addrs [MESHUNITS-1:0],
        output C_write_valid [MESHUNITS-1:0]
    );

    // COMP STATE
    wire COMP_LOCK_FREE = ~comp_lock[0] & ~comp_lock[1];
    wire COMP_LOCK_ZERO = comp_lock[0];
    wire COMP_LOCK_ONE = comp_lock[1];
    reg comp_lock [1:0];
    reg [BITWIDTH-1:0] comp_tick_ctr;
    reg comp_complete;
    reg [BITWIDTH-1:0] A_base_addr;
    reg [BITWIDTH-1:0] D_base_addr;
    reg [BITWIDTH-1:0] C_base_addr;

    // COMP MEMORY INPUT SIGNALS
    reg [BITWIDTH-1:0] A_row_read_addrs_buffer [MESHUNITS-1:0];
    reg [BITWIDTH-1:0] D_col_read_addrs_buffer [MESHUNITS-1:0];
    reg A_read_valid_buffer [MESHUNITS-1:0];
    reg D_read_valid_buffer [MESHUNITS-1:0];

    // COMP MEMORY OUTPUT SIGNALS
    reg [BITWIDTH-1:0] C_buffer [MESHUNITS-1:0][TILEUNITS-1:0];
    reg [BITWIDTH-1:0] C_col_write_addrs_buffer [MESHUNITS-1:0];
    reg C_write_valid_buffer [MESHUNITS-1:0];

    // COMP ARRAY INPUT SIGNALS
    reg array_A_valid [MESHUNITS-1:0][TILEUNITS-1:0];
    reg array_D_valid [MESHUNITS-1:0][TILEUNITS-1:0];

    // COMP ARRAY OUTPUT SIGNALS
    reg [BITWIDTH-1:0] array_C [MESHUNITS-1:0][TILEUNITS-1:0];
    reg array_C_valid [MESHUNITS-1:0][TILEUNITS-1:0];

    // LOAD STATE
    wire LOAD_LOCK_FREE = ~load_lock[0] & ~load_lock[1];
    wire LOAD_LOCK_ZERO = load_lock[0];
    wire LOAD_LOCK_ONE = load_lock[1];
    reg load_lock [1:0];
    reg [BITWIDTH-1:0] load_tick_ctr;
    reg load_complete;
    reg [BITWIDTH-1:0] B_base_addr;

    // LOAD MEMORY SIGNALS
    reg [BITWIDTH-1:0] B_col_read_addrs_buffer [MESHUNITS-1:0];
    reg B_read_valid_buffer [MESHUNITS-1:0];

    // LOAD ARRAY INPUT SIGNALS
    reg B_valid [MESHUNITS-1:0][TILEUNITS-1:0];
    reg [BITWIDTH-1:0] B_shelf_life [MESHUNITS-1:0][TILEUNITS-1:0];
    reg B_propagate [MESHUNITS-1:0][TILEUNITS-1:0];



    always @(*) begin

        //
        // COMP LOGIC
        //
        integer i, j;
        if (COMP_LOCK_FREE) begin
            for (i = 0; i < MESHUNITS; i++) begin
                A_row_read_addrs_buffer[i] = 0;
                D_col_read_addrs_buffer[i] = 0;
                A_read_valid_buffer[i] = 0;
                D_read_valid_buffer[i] = 0;
                for (j = 0; j < TILEUNITS; j++) begin
                    array_A_valid[i][j] = 0;
                    array_D_valid[i][j] = 0;
                end
                
                C_col_write_addrs_buffer[i] = 0;
                C_write_valid_buffer[i] = 0;
                for (j = 0; j < TILEUNITS; j++) begin
                    C_buffer[i][j] = 0;
                end
            end
        end
        else begin

            // READ COMP (A, D) SIGNALS: MEM + ARRAY
            for (i = 0; i < MESHUNITS; i++) begin
                // row/col i of the sys array receives an input signal
                // iff k <= i < k + (MU * TU)
                // i.e., row/col i starts reading MU * TU values at counter k = i
                if (comp_tick_ctr >= i && comp_tick_ctr < (MESHUNITS * TILEUNITS) + i) begin
                    // at counter k read from addr:
                    // A/D[k - i][i] = A/D_base_addr + (k - i) * (MU * TU) + i * (TU)
                    A_row_read_addrs_buffer[i] = A_base_addr + (comp_tick_ctr - i) * MESHUNITS * TILEUNITS + i * TILEUNITS;
                    D_col_read_addrs_buffer[i] = D_base_addr + (comp_tick_ctr - i) * MESHUNITS * TILEUNITS + i * TILEUNITS;
                    A_read_valid_buffer[i] = 1;
                    D_read_valid_buffer[i] = 1;
                    for (j = 0; j < TILEUNITS; j++) begin
                        array_A_valid[i][j] = 1;
                        array_D_valid[i][j] = 1;
                    end
                    
                end
                else begin
                    A_row_read_addrs_buffer[i] = 0;
                    D_col_read_addrs_buffer[i] = 0;
                    A_read_valid_buffer[i] = 0;
                    D_read_valid_buffer[i] = 0;
                    for (j = 0; j < TILEUNITS; j++) begin
                        array_A_valid[i][j] = 0;
                        array_D_valid[i][j] = 0;
                    end
                end
            end

            // WRITE ADDR + VALID (C) SIGNALS: MEM
            for (i = 0; i < MESHUNITS; i++) begin
                // col i of the sys array produces a valid output signal
                // iff (k - MU) <= i < (k - MU) + (MU * TU)
                // i.e., col i start writing MU * TU values at counter k = i + MU
                // note that this is MU greater than the read signal start MU cycles to propagate
                if (comp_tick_ctr >= (MESHUNITS + i) && comp_tick_ctr < ((MESHUNITS + i) + (MESHUNITS * TILEUNITS))) begin
                    C_col_write_addrs_buffer[i] = C_base_addr + (comp_tick_ctr - (MESHUNITS + i)) * MESHUNITS * TILEUNITS + i * TILEUNITS;
                    C_write_valid_buffer[i] = 1;
                    for (j = 0; j < TILEUNITS; j++) begin
                        C_buffer[i][j] = array_C[i][j];
                        if (!array_C_valid[i][j])
                            C_write_valid_buffer[i] = 0;
                    end
                end
                else begin
                    C_col_write_addrs_buffer[i] = 0;
                    C_write_valid_buffer[i] = 0;
                    for (j = 0; j < TILEUNITS; j++) begin
                        C_buffer[i][j] = 0;
                    end
                end
            end
        end

        // COMP COMPLETE on the cycle after the final C write goes through
        // --> k = ((MU + i) + (MU * TU)) + 1, i = final col (MU - 1)
        // --> k = ((MU + MU - 1)) + (MU * TU) + 1
        // --> k = MU * (2 + TU)
        comp_complete = comp_tick_ctr == MESHUNITS * (2 + TILEUNITS) - 1;

        //
        // LOAD LOGIC
        //

        if (LOAD_LOCK_FREE) begin
            for (i = 0; i < MESHUNITS; i++) begin
                B_col_read_addrs_buffer[i] = 0;
                B_read_valid_buffer[i] = 0;
                for (j = 0; j < TILEUNITS; j++) begin
                    B_valid[i][j] = 0;
                    B_shelf_life[i][j] = 0;
                end
            end
        end
        else begin
            // READ LOAD (B) SIGNALS: MEM + ARRAY
            for (i = 0; i < MESHUNITS; i++) begin
                // col i of the sys array receives an input signal
                // iff l <= i < l + (MU * TU)
                // i.e., col i starts reading MU * TU values at counter l = i
                if (load_tick_ctr >= i && load_tick_ctr < (MESHUNITS * TILEUNITS) + i) begin
                    // at counter l read from addr:
                    // B[(MU * TU - 1) - (k - i)][i] = B_base_addr + (k - i) * (MU * TU) + i * (TU)
                    B_col_read_addrs_buffer[i] = B_base_addr + ((MESHUNITS * TILEUNITS - 1) - (load_tick_ctr - i)) * MESHUNITS * TILEUNITS + i * TILEUNITS;
                    B_read_valid_buffer[i] = 1;
                    for (j = 0; j < TILEUNITS; j++) begin
                        B_valid[i][j] = 1;
                        B_shelf_life[i][j] = (MESHUNITS * TILEUNITS) - (load_tick_ctr - i);
                    end
                end
                else begin
                    B_col_read_addrs_buffer[i] = 0;
                    B_read_valid_buffer[i] = 0;
                    for (j = 0; j < TILEUNITS; j++) begin
                        B_valid[i][j] = 0;
                        B_shelf_life[i][j] = 0;
                    end
                end
            end         
        end
        // LOAD COMPLETE on the cycle after last col/last row loaded into sys array
        // --> l = MU * TU + i + 1, i = final col (MU - 1)
        // --> l = MU * TU + MU - 1 + 1
        // --> l = MU * (1 + TU)
        load_complete = load_tick_ctr == MESHUNITS * (1 + TILEUNITS);

        // 
        // GLOBAL ARRAY LOGIC (PROPAGATE)
        //
        for (i = 0; i < MESHUNITS; i++) begin
            for (j = 0; j < TILEUNITS; j++) begin
                if (LOAD_LOCK_ZERO | COMP_LOCK_ONE) begin
                    B_propagate[i][j] = 0;
                end
                else begin
                    B_propagate[i][j] = 1;
                end
            end
        end
    end

    assign comp_finished = comp_complete;
    assign load_finished = load_complete;
    assign A_row_read_addrs = A_row_read_addrs_buffer;
    assign D_col_read_addrs = D_col_read_addrs_buffer;
    assign B_col_read_addrs = B_col_read_addrs_buffer;
    assign A_read_valid = A_read_valid_buffer;
    assign D_read_valid = D_read_valid_buffer;
    assign B_read_valid = B_read_valid_buffer;
    assign C = C_buffer;
    assign C_col_write_addrs = C_col_write_addrs_buffer;
    assign C_write_valid = C_write_valid_buffer;

    always @(posedge clock) begin
        integer i;
        if (reset) begin
            for (i = 0; i < 2; i++) begin
                comp_lock[i] <= 0;
                load_lock[i] <= 0;
            end
        end
        else begin
            //
            // COMP/LOAD COUNTER LOGIC
            //
            if (~COMP_LOCK_FREE)
                comp_tick_ctr <= comp_tick_ctr + 1;
            if (~LOAD_LOCK_FREE)
                load_tick_ctr <= load_tick_ctr + 1;

            //
            // SYNCHRONIZATION LOGIC
            //
            if (comp_complete) begin
                for (i = 0; i < 2; i++)
                    comp_lock[i] <= 0;
            end
            if (load_complete) begin
                for (i = 0; i < 2; i++)
                    load_lock[i] <= 0;
            end
            if (COMP_LOCK_FREE && LOAD_LOCK_FREE) begin
                if (comp_lock_req[0]) begin
                    // assign comp_lock to 0 and try to assign load_lock to 1
                    comp_lock[0] <= 1;
                    comp_tick_ctr <= 0;
                    A_base_addr <= A_addr[0];
                    D_base_addr <= D_addr[0];
                    C_base_addr <= C_addr[0];
                    if (load_lock_req[1]) begin
                        load_lock[1] <= 1;
                        B_base_addr <= B_addr[1];
                    end
                    else begin
                        for (i = 0; i < 2; i++)
                            load_lock[i] <= 0;
                    end
                end
                else if (comp_lock_req[1]) begin
                    // assign comp_lock to 1 and try to assign load lock to 0
                    comp_lock[1] <= 1;
                    comp_tick_ctr <= 0;
                    A_base_addr <= A_addr[1];
                    D_base_addr <= D_addr[1];
                    C_base_addr <= C_addr[1];
                    if (load_lock_req[0]) begin
                        load_lock[0] <= 1;
                        B_base_addr <= B_addr[0];
                    end
                    else begin
                        for (i = 0; i < 2; i++)
                            load_lock[i] <= 0;
                    end
                end
                else begin
                    // assign load lock to lowest index requester
                    for (i = 0; i < 2; i++)
                        comp_lock[i] <= 0;
                    if (load_lock_req[0]) begin
                        load_lock[0] <= 1;
                        B_base_addr <= B_addr[0];
                    end
                    else if (load_lock_req[1]) begin
                        load_lock[1] <= 1;
                        B_base_addr <= B_addr[1];
                    end
                    else begin
                        for (i = 0; i < 2; i++)
                            load_lock[i] <= 0;
                    end
                end
            end
            else if (COMP_LOCK_FREE) begin
                // try to assign comp lock to first thread that doesn't hold load lock
                if (~load_lock[0] && comp_lock_req[0]) begin
                    comp_lock[0] <= 1;
                    comp_tick_ctr <= 0;
                    A_base_addr <= A_addr[0];
                    D_base_addr <= D_addr[0];
                    C_base_addr <= C_addr[0];
                end
                else if (~load_lock[1] && comp_lock_req[1]) begin
                    comp_lock[1] <= 1;
                    comp_tick_ctr <= 0;
                    A_base_addr <= A_addr[1];
                    D_base_addr <= D_addr[1];
                    C_base_addr <= C_addr[1];
                end
            end
            else if (LOAD_LOCK_FREE) begin
                // try to assign load lock to first thread that doesn't hold comp lock
                if (~comp_lock[0] && load_lock_req[0]) begin
                    load_lock[0] <= 1;
                    B_base_addr <= B_addr[0];
                end
                else if (~comp_lock[1] && load_lock_req[1]) begin
                    load_lock[1] <= 1;
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
    
    sys_array #(MESHUNITS, MESHUNITS, BITWIDTH, TILEUNITS, TILEUNITS)
    sys_array_module (
        .clock(clock),
        .reset(reset),
        .in_dataflow(1),
        .in_a(A),
        .in_a_valid(array_A_valid),
        .in_b(B),
        .in_d(D),
        .in_propagate(B_propagate),
        .in_b_shelf_life(B_shelf_life),
        .in_b_valid(B_valid),
        .in_d_valid(array_D_valid),
        .out_c(array_C),
        .out_c_valid(array_C_valid)
    );

endmodule
