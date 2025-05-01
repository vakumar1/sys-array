## Systolic Array MatMul Accelerator

Accelerator for matrix multiplication - the accelerator loads instructions + matrix memory from the host and executes the instructions to compute matrix multiplications and send the output back to the host

Notes on the accelerator design and ISA are linked below
- [x] ![Systolic Array MatMul Device Part 1: The Systolic Array](https://hackmd.io/@n9vXJ2dWSK-txnWnjmMPGQ/BJ3wJUzEke)
- [x] ![Systolic Array MatMul Device Part 2: UART Comms + Memory](https://hackmd.io/@n9vXJ2dWSK-txnWnjmMPGQ/HJsfYs8o6)
- [x] ![Systolic Array MatMul Device Part 3: ISA](https://hackmd.io/@n9vXJ2dWSK-txnWnjmMPGQ/SylWqNBEyx)

### Virtual Accelerator Usage
- build the virtual accelerator with Verilator and compile - note this requires the Verilator dependency to be installed locally ![here](https://github.com/vakumar1/sys-array/blob/main/Makefile#L2-L3)
```
make core
```
- build the host driver with attached virtual accelerator
```
make driver
```
- run the driver + accelerator
```
./driver [<ISA files>]
```

### ISA .txt Files

Example ISA files can be found under the [integration tests](https://github.com/vakumar1/sys-array/tree/main/software/integration)

ISA files must contain 3 sections:

#### META
2 positive integer parameters denoting the **tile size** and **mesh size** of the virtual accelerator configured [here](https://github.com/vakumar1/sys-array/blob/main/Makefile#L53-L56)

Example:
```
===META
2 2
```

#### DATA
A list of matrices that will be loaded into the accelerator before the script is loaded - each matrix contains 2 lines followed by a newline
- line 1 contains a **human-readable matrix name** and the **hex device address** of where the matrix will be loaded
- line 2 contains a space-delimited list of **32b integer entries** of the matrix

Example:
```
===DATA
B 0x00000800
1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1

A1 0x00000900
-3 -2 -1 0 1 2 3 4 9 8 7 6 -6 -7 -8 -9

```

#### TEXT
A list of ISA instructions that will be loaded into the accelerator and run - a complete specification for the ISA can be found in the notes above
