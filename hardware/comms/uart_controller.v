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
    wire write_ready;

    // UART STATE
    wire [7:0] uart_data_in = WRITE_LOCK_FREE
                            ? 0
                            : WRITE_LOCK_ZERO
                                ? data_in[0]
                                : data_in[1];
    wire uart_data_in_valid = WRITE_LOCK_FREE
                            ? 0
                            : WRITE_LOCK_ZERO
                                ? data_in_valid[0]
                                : data_in_valid[1];

    // UART->FIFO SIGNALS
    wire [7:0] last_uart_read_byte;
    wire last_uart_read_valid;

    // FIFO STATE
    reg fifo_empty;
    reg fifo_full;

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

    uart #(BAUD_RATE, CLOCK_FREQ)
    _uart (
        .clock(clock),
        .reset(reset),

        // write data directly to UART (if lock acq. and valid)
        .tx_ready(write_ready),
        .data_in(uart_data_in),
        .data_in_valid(uart_data_in_valid),

        // read data from UART directly to FIFO if FIFO not full
        .data_out(last_uart_read_byte),
        .data_out_valid(last_uart_read_valid),
        .serial_in(serial_in),
        .serial_out(serial_out),
        .local_ready(~fifo_full),
        .cts(1), // TODO: use control signals from driver
        .rts() // TODO: use control signals from driver
    );

    fifo #(BUFFER_SIZE)
    _fifo (
        .clock(clock),
        .reset(reset),

        // attempt to write data from UART to FIFO
        .write(last_uart_read_valid),
        .data_in(last_uart_read_byte),

        // attempt to read data from FIFO
        .read(read_valid),
        .data_out(data_out),
        .data_out_valid(data_out_valid),
        .empty(fifo_empty),
        .full(fifo_full)
    );

endmodule
