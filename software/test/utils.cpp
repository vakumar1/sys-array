#include "utils.h"

#include <stdexcept>
#include <string>

void signal_err(std::string signal, int expected, int actual) {
    if (expected == actual)
        return;
    std::string msg = "[" + std::string(__FILE__) + ":" + std::to_string(__LINE__) + "]"
            " signal=" + signal + "]"
            + " expected=" + std::to_string(expected) + " actual=" + std::to_string(actual);
    throw std::runtime_error(msg);
}

void uart_signal_err(int iter, int tick, std::string signal, int expected, int actual) {
    if (expected == actual)
        return;
    std::string msg = "[" + std::string(__FILE__) + ":" + std::to_string(__LINE__) + "]"
            + "[iter=" + std::to_string(iter) + " tick=" + std::to_string(tick) + " signal=" + signal + "]" 
            + " expected=" + std::to_string(expected) + " actual=" + std::to_string(actual);
    throw std::runtime_error(msg);
}

void data_err(std::string var, char expected, char actual) {
    if (expected == actual)
        return;
    std:: string msg = "[var=" + var + "]" + 
            + " expected=" + std::to_string(static_cast<int>(expected)) 
            + " actual=" + std::to_string(static_cast<int>(actual));
    throw std::runtime_error(msg);
}
