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
        input write_valid
    );

    reg signed [BITWIDTH-1:0] instr_mem [ADDRSIZE-1:0] /*verilator public*/; 

    assign read_instr1 = instr_mem[read_addr1 >> 2];
    assign read_instr2 = instr_mem[read_addr2 >> 2];

    // sync write signals
    always @(posedge clock) begin
        if (reset) begin
            instr_mem <= '{default: '0};
        end
        else begin
            if (write_valid) begin
                instr_mem[write_addr >> 2] <= write_data;
            end
        end
    end
endmodule
