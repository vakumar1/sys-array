#include "verilated.h"
#include "verilated_vcd_c.h"

#include <string>
#include <functional>

// RETURN VALUES
#define SUCCESS 0
#define SIM_ERROR 1
#define INTERNAL_ERROR 2

// error checkers
void signal_err(std::string signal, int expected, int actual);
void uart_signal_err(int iter, int tick, std::string signal, int expected, int actual);
void data_err(std::string var, char expected, char actual);
void condition_err(std::string condition_msg, bool condition);

// graceful test runner
void test_runner(std::string test_header, std::string test_name, std::function<void()> test, std::function<void()> error_hook);