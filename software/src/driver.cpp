#include "utils/instr_utils.h"
#include "virtual_device.h"

#include <regex>
#include <unordered_map>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <fstream>

void driver_log(std::string header, std::string msg) {
    std::cout << "[" + header + "] " + msg << std::endl;
}

void matrix_log(std::string header, std::array<int, MESHUNITS * MESHUNITS * TILEUNITS * TILEUNITS>& data) {
    for (unsigned i = 0; i < MESHUNITS * TILEUNITS; i++) {
        std::string line("");
        for (unsigned j = 0; j < MESHUNITS * TILEUNITS; j++) {
            line += std::to_string(data[i * MESHUNITS * TILEUNITS + j]);
            line += " ";
        }
        line = "[ " + line + " ]";
        driver_log(header, line);
    }
}

typedef struct {
    std::unordered_map<std::string, unsigned int> data_addresses;
    std::unordered_map<std::string, std::array<int, MESHUNITS * MESHUNITS * TILEUNITS * TILEUNITS>> data;
    std::vector<instr_t> instructions;
} script_t;

#define SECTION_DELIMITER std::string("===")
#define META_HEADER std::string("META")
#define DATA_HEADER std::string("DATA")
#define TEXT_HEADER std::string("TEXT")
#define TERM_INST std::string("TERM")
#define WRITE_INST std::string("WRITE")
#define LOAD_INST std::string("LOAD")
#define COMP_INST std::string("COMP") 

void parse_meta(std::string input) {
    std::istringstream iss(input);
    std::vector<std::string> tokens;
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    if (tokens[0] != META_HEADER) {
        throw std::runtime_error("Unexpected error - tried to parse non META section as META");
    }
    std::vector<std::string> subtokens(tokens.begin() + 1, tokens.end());

    if (subtokens.size() != 2) {
        throw std::runtime_error("META - section requires exactly 2 int params");
    }
    int configured_meshunits = std::stoi(subtokens[0]);
    int configured_tileunits = std::stoi(subtokens[1]);
    if (configured_meshunits != MESHUNITS) {
        throw std::runtime_error("Invalid mesh units: got " + std::to_string(configured_meshunits) + " expected " + std::to_string(MESHUNITS));
    }
    if (configured_tileunits != TILEUNITS) {
        throw std::runtime_error("Invalid tile units: got " + std::to_string(configured_tileunits) + " expected " + std::to_string(TILEUNITS));
    }
    driver_log(META_HEADER, std::string("Using parameters MESHUNITS=" + subtokens[0] + " TILEUNITS=" + subtokens[1]));
}

void parse_data(std::string input,
                std::unordered_map<std::string, unsigned int>& address_map,
                std::unordered_map<std::string, std::array<int, MESHUNITS * MESHUNITS * TILEUNITS * TILEUNITS>>& data_map) {
    std::istringstream iss(input);
    std::vector<std::string> tokens;
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    if (tokens[0] != DATA_HEADER) {
        throw std::runtime_error("Unexpected error - tried to parse non DATA section as DATA");
    }
    std::vector<std::string> subtokens(tokens.begin() + 1, tokens.end());
    
    bool parsing_name = true;
    bool parsing_address = true;
    std::string curr_name;
    std::vector<int> curr_matrix;
    for (std::string tok : subtokens) {
        if (parsing_name) {
            if (data_map.count(tok) > 0) {
                throw std::runtime_error("DATA has multiple matrices defined as " + tok);
            }
            parsing_name = false;
            curr_name = tok;
        } else if (parsing_address) {
            parsing_address = false;
            address_map[curr_name] = std::stoi(tok, nullptr, 16);
        } else {
            int val = std::stoi(tok);
            curr_matrix.push_back(val);
            if (curr_matrix.size() == MESHUNITS * MESHUNITS * TILEUNITS * TILEUNITS) {
                parsing_name = true;
                parsing_address = true;
                std::array<int, MESHUNITS * MESHUNITS * TILEUNITS * TILEUNITS> arr = {0};
                std::copy(curr_matrix.begin(), curr_matrix.end(), arr.begin());
                data_map[curr_name] = arr;
                curr_matrix.clear();
                driver_log(DATA_HEADER, std::string("Added matrix=" + curr_name));
                matrix_log(DATA_HEADER, arr);
            }
        }
    }
    if (curr_matrix.size() > 0) {
        throw std::runtime_error("DATA section has incomplete matrix");
    }

}

void parse_text(std::string input,
                std::vector<instr_t>& inst_list) {
    std::istringstream iss(input);
    std::vector<std::string> tokens;
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    if (tokens[0] != TEXT_HEADER) {
        throw std::runtime_error("Unexpected error - tried to parse non TEXT section as TEXT");
    }
    std::vector<std::string> subtokens(tokens.begin() + 1, tokens.end());

    unsigned int index = 0;
    unsigned int inst_count = 0;
    while (index < subtokens.size()) {
        if (subtokens[index] == TERM_INST) {
            instr_t inst;
            inst.type = TERM;
            inst.inner_instr.t = {};
            inst_list.push_back(inst);
            index += 1;
        } else if (subtokens[index] == WRITE_INST) {
            if (index + 3 > subtokens.size()) {
                throw std::runtime_error("TEXT section has incomplete instruction");
            }
            unsigned char address = (unsigned char) (((std::stoi(subtokens[index + 1], nullptr, 16)) >> 8) & 0xFF);
            unsigned char header = (unsigned char) std::stoi(subtokens[index + 2], nullptr, 16) & 0xFF;
            instr_t inst;
            inst.type = WRITE;
            inst.inner_instr.w = { header, address };
            inst_list.push_back(inst);
            index += 3;
        } else if (subtokens[index] == LOAD_INST) {
            if (index + 2 > subtokens.size()) {
                throw std::runtime_error("TEXT section has incomplete instruction");
            }
            unsigned char address = (unsigned char) (((std::stoi(subtokens[index + 1], nullptr, 16)) >> 8) & 0xFF);
            instr_t inst;
            inst.type = LOAD;
            inst.inner_instr.l = { address };
            inst_list.push_back(inst);
            index += 2;
        } else if (subtokens[index] == COMP_INST) {
            if (index + 4 > subtokens.size()) {
                throw std::runtime_error("TEXT section has incomplete instruction");
            }
            unsigned char a_addr = (unsigned char) (((std::stoi(subtokens[index + 1], nullptr, 16)) >> 8) & 0xFF);
            unsigned char d_addr = (unsigned char) (((std::stoi(subtokens[index + 2], nullptr, 16)) >> 8) & 0xFF);
            unsigned char c_addr = (unsigned char) (((std::stoi(subtokens[index + 3], nullptr, 16)) >> 8) & 0xFF);
            instr_t inst;
            inst.type = COMP;
            inst.inner_instr.c = { a_addr, d_addr, c_addr };
            inst_list.push_back(inst);
            index += 4;
        } else {
            throw std::runtime_error("Unrecognized instruction " + subtokens[index]);
        }
        inst_count += 1;
    }
}


script_t parse_script(std::string input) {
    std::vector<std::string> sections;
    size_t start = input.find(SECTION_DELIMITER);
    while (start != std::string::npos) {
        size_t end = input.find(SECTION_DELIMITER, start + 1);
        if (end != std::string::npos) {
            sections.push_back(input.substr(start + SECTION_DELIMITER.size(), end - (start + SECTION_DELIMITER.size())));
        } else {
            sections.push_back(input.substr(start + SECTION_DELIMITER.size()));
        }
        start = end;
    }

    unsigned int meta_section_count = 0;
    unsigned int data_section_count = 0;
    unsigned int text_section_count = 0;
    std::unordered_map<std::string, unsigned int> address_map;
    std::unordered_map<std::string, std::array<int, MESHUNITS * MESHUNITS * TILEUNITS * TILEUNITS>> data_map;
    std::vector<instr_t> inst_list;
    for (std::string section : sections) {
        if (section.compare(0, META_HEADER.size(), META_HEADER) == 0) {
            parse_meta(section);
            meta_section_count++;
        } else if (section.compare(0, DATA_HEADER.size(), DATA_HEADER) == 0) {
            parse_data(section, address_map, data_map);
            data_section_count++;
        } else if (section.compare(0, TEXT_HEADER.size(), TEXT_HEADER) == 0) {
            parse_text(section, inst_list);
            text_section_count++;
        }
    }
    return { address_map, data_map, inst_list };
}

void run_script(std::string file_path, virtual_device* device) {
    std::ifstream file(file_path, std::ios::in | std::ios::binary);
    if (!file) {
        throw std::ios_base::failure("Error opening file");
    }
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    script_t script = parse_script(content);

    // store bmem data
    for (const auto& pair : script.data_addresses) {
        std::string matrix_name = pair.first;
        unsigned int address = pair.second;
        device->block_store(address, script.data[matrix_name]);
        driver_log(std::string("LOAD_BMEM"), std::string("ADDRESS: ") + print_hex_int(address));
        matrix_log(std::string("LOAD_BMEM"), script.data[matrix_name]);
    }

    // store imem data
    unsigned int imem_addr = 0x0;
    for (instr_t instr : script.instructions) {
        unsigned int imem_data;
        switch (instr.type) {
            case TERM:
                imem_data = term_instr_to_bits(instr.inner_instr.t);
                break;
            case WRITE:
                imem_data = write_instr_to_bits(instr.inner_instr.w);
                break;
            case LOAD:
                imem_data = load_instr_to_bits(instr.inner_instr.l);
                break;
            case COMP:
                imem_data = comp_instr_to_bits(instr.inner_instr.c);
                break;
            default:
                throw std::runtime_error("Unaccepted instruction type");
        }
        driver_log(std::string("LOAD_IMEM"), print_hex_int(imem_addr) + std::string(" ") + print_instr(instr));
        device->imem_store(imem_addr, imem_data);
        imem_addr += 0x4;
    }

    // start thread and wait forever
    std::array<bool, 4> update = {1, 1, 0, 0};
    device->thread_update(update);
    unsigned char header;
    std::array<int, MESHUNITS * MESHUNITS * TILEUNITS * TILEUNITS> data;
    device->read_bmem(header, data);
    driver_log(std::string("READ_BMEM"), std::string("HEADER: ") + std::to_string(header));
    matrix_log(std::string("READ_BMEM"), data);
    update = {0, 0, 0, 0};
    device->thread_update(update);

}

int main(int argc, char** argv) {    

    // parse script
    std::vector<std::string> files;
    for (int i = 1; i < argc; i++) {
        files.push_back(std::string(argv[i]));
    }

    // setup virtual device
    Verilated::commandArgs(argc, argv);
    Vcore* core = new Vcore;
    Vuart* driver_uart = new Vuart;
    Verilated::traceEverOn(true);
    virtual_device* device = new virtual_device;
    device->init_device(driver_uart, core);

    for (std::string file_path : files) {
        driver_log(std::string("DRIVER"), std::string("Running script: ") + file_path);
        run_script(file_path, device);
    }
    driver_log(std::string("DRIVER"), std::string("Finished running scripts - exiting"));
}
