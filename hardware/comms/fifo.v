module fifo
    #(BUFFER_SIZE=256)
    (
        input clock,
        input reset,

        input write,
        input [7:0] data_in,

        input read, 
        output [7:0] data_out,
        output data_out_valid,

        output empty,
        output full
    );

    reg [7:0] buffer [BUFFER_SIZE-1:0];
    reg [31:0] buffer_start;
    reg [31:0] buffer_count;
    reg [7:0] last_read;
    reg last_read_valid;
    wire write_successful = write && buffer_count < BUFFER_SIZE;
    wire read_successful = read && buffer_count > 0;
    always @(posedge clock) begin
        if (reset) begin
            buffer_start <= 0;
            buffer_count <= 0;
            last_read_valid <= 0;
        end
        else begin
            // write iff there is space
            if (write_successful) begin
                buffer[
                    buffer_start + buffer_count >= BUFFER_SIZE 
                        ? buffer_start + buffer_count - BUFFER_SIZE
                        : buffer_start + buffer_count
                ] <= data_in;
            end

            // read iff there is data to read
            if (read_successful) begin
                buffer_start <= buffer_start == BUFFER_SIZE - 1
                                    ? 0
                                    : buffer_start + 1;
                last_read <= buffer[buffer_start];
                last_read_valid <= 1;
            end
            else begin
                last_read_valid <= 0;
            end

            if (write_successful && !read_successful)
                buffer_count <= buffer_count + 1;
            if (!write_successful && read_successful)
                buffer_count <= buffer_count - 1;
        end
    end
    assign data_out = last_read;
    assign data_out_valid = last_read_valid;
    assign empty = buffer_count == 0;
    assign full = buffer_count == BUFFER_SIZE;
endmodule
