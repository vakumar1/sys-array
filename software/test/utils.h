#include <string>

// RETURN VALUES
#define SUCCESS 0
#define SIM_ERROR 1
#define INTERNAL_ERROR 2

// error checkers
void signal_err(std::string signal, int expected, int actual);
void uart_signal_err(int iter, int tick, std::string signal, int expected, int actual);
void data_err(std::string var, char expected, char actual);
