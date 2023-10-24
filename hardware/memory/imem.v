module imem
    #(
        parameter ADDRSIZE, BITWIDTH
    )
    (
        input clock,
        input reset,

        // READ SIGNALS
        input [BITWIDTH-1:0] read_addr1,
        input [BITWIDTH-1:0] read_addr2,
        output [BITWIDTH-1:0] read_instr1,
        output [BITWIDTH-1:0] read_instr2,

        // WRITE SIGNALS
        input [BITWIDTH-1:0] write_addr,
        input [BITWIDTH-1:0] write_data,
        input [BITWIDTH-1:0] write_valid
    );

    reg signed [BITWIDTH-1:0] instr_mem [ADDRSIZE-1:0];

    assign instr_addr_mask = {(BITWIDTH - $clog2(BITWIDTH >> 3)){1'b1}, $clog2(BITWIDTH >> 3){1'b0}};
    wire instr_addr1 = read_addr1 & instr_addr_mask;
    wire instr_addr2 = read_addr2 & instr_addr_mask;
    wire write_instr_addr = write_addr & instr_addr_mask;

    // async read signals
    assign read_instr1 = instr_mem[instr_addr1 + BITWIDTH - 1:instr_addr1];
    assign read_instr2 = instr_mem[instr_addr2 + BITWIDTH - 1:instr_addr2];

    // sync write signals
    always @(posedge clock) begin
        if (reset) begin
            integer i;
            for (i = 0; i < ADDRSIZE; i++) begin
                instr_mem[i] <= 0;
            end
        end
        else begin
            if (write_valid) begin
                instr_mem[write_instr_addr] <= write_data;
            end
        end
    end
endmodule
