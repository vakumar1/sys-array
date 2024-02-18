module uart_controller
    #
    (
        parameter BAUD_RATE=115_200, CLOCK_FREQ=125_000_000, BUFFER_SIZE=256
    )
    (
        input clock,
        input reset,

        // CONTROL SIGNALS
        input write_lock_req [1:0],
        output write_lock_res [1:0],

        // WRITE SIGNALS (sync)
        output write_ready,
        input [7:0] data_in [1:0],
        input data_in_valid [1:0],

        // READ SIGNALS (sync)
        input read_valid,
        output [7:0] data_out,
        output data_out_valid,

        // SERIAL LINES
        input serial_in,
        output serial_out

    );

    // SYNCHRONIZATION SIGNALS + STATE
    wire WRITE_LOCK_FREE = ~write_lock[0] & ~write_lock[1];
    wire WRITE_LOCK_ZERO = write_lock[0];
    wire WRITE_LOCK_ONE = write_lock[1];
    reg write_lock [1:0];

    always @(posedge clock) begin
        if (reset) begin
            write_lock[0] <= 0;
            write_lock[1] <= 0;
        end
        else if (write_ready) begin

            // UPDATE WRITE LOCK SYNCH. (only if UART is ready to write)
            if (WRITE_LOCK_FREE) begin
                if (write_lock_req[0]) begin
                    write_lock[0] <= 1;
                    write_lock[1] <= 0;
                end
                else if (write_lock_req[1]) begin
                    write_lock[0] <= 0;
                    write_lock[1] <= 1;
                end
                else begin
                    write_lock[0] <= 0;
                    write_lock[1] <= 0;
                end
            end
            else if (WRITE_LOCK_ZERO) begin
                if (~write_lock_req[0]) begin
                    write_lock[0] <= 0;
                    write_lock[1] <= write_lock_req[1] ? 1 : 0;
                end
                else begin
                    write_lock[0] <= 1;
                    write_lock[1] <= 0;
                end
            end
            else if (WRITE_LOCK_ONE) begin
                if (~write_lock_req[1]) begin
                    write_lock[0] <= write_lock_req[0] ? 1 : 0;
                    write_lock[1] <= 0;
                end
                else begin
                    write_lock[0] <= 0;
                    write_lock[1] <= 1;
                end
            end
        end
    end

    assign write_lock_res = write_lock;

    // UART write signals
    reg uart_write_ready;
    reg [7:0] uart_data_in;
    reg uart_data_in_valid;
    assign write_ready = uart_write_ready;

    // UART read signals
    reg [7:0] uart_data_out;
    reg uart_data_out_valid;

    uart #(BAUD_RATE, CLOCK_FREQ)
    _uart (
        .clock(clock),
        .reset(reset),

        // write data from W_FIFO -> UART
        .tx_ready(uart_write_ready),
        .data_in(uart_data_in),
        .data_in_valid(uart_data_in_valid),

        // read data from UART to R_FIFO
        .data_out(uart_data_out),
        .data_out_valid(uart_data_out_valid),
        .serial_in(serial_in),
        .serial_out(serial_out),
        .local_ready(1), // TODO: use R_FIFO signals to control UART reads
        .cts(1), // TODO: use control signals from driver
        .rts() // TODO: use control signals from driver
    );


    // W_FIFO signals
    reg write_fifo_empty;
    reg write_fifo_full;

    // filtered (lock-checked) INPUT->W_FIFO write data
    wire [7:0] filtered_data_in = WRITE_LOCK_FREE
                                    ? 0
                                    : WRITE_LOCK_ZERO
                                        ? data_in[0]
                                        : data_in[1];
    wire filtered_data_in_valid = WRITE_LOCK_FREE
                                    ? 0
                                    : WRITE_LOCK_ZERO
                                        ? data_in_valid[0]
                                        : data_in_valid[1];
    fifo #(BUFFER_SIZE)
    _write_fifo (
        .clock(clock),
        .reset(reset),

        // attempt to write data from input to FIFO
        .write(filtered_data_in_valid),
        .data_in(filtered_data_in),

        // connect W_FIFO to UART data in (if UART ready)
        .read(uart_write_ready),
        .data_out(uart_data_in),
        .data_out_valid(uart_data_in_valid),
        .empty(write_fifo_empty),
        .full(write_fifo_full)
    );

    // R_FIFO signals
    reg read_fifo_empty;
    reg read_fifo_full;

    fifo #(BUFFER_SIZE)
    _read_fifo (
        .clock(clock),
        .reset(reset),

        // attempt to write data from UART data out to FIFO
        .write(uart_data_out_valid),
        .data_in(uart_data_out),

        // attempt to read data from FIFO to output
        .read(read_valid),
        .data_out(data_out),
        .data_out_valid(data_out_valid),
        .empty(read_fifo_empty),
        .full(read_fifo_full)
    );

endmodule
