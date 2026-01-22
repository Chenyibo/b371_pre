# Context
This C program is a simple simulator of the MIPS Processor Architecture. This is a multicycle design but is not parallelised.

# How to use
This program generates an object file that when run requires three parameters.

1. Task Number: 1=InstructionType, 2=FSMDecode, 3=FSM, 4=FSMExit, 5=FullOperation
2. Relative path to the initial memory file
3. Relative path to the initial register file

# How to build
This project uses Makefiles to compile

1. Run `make` in a Unix terminal
2. Run the generated `mips-processor-simulator` file with the supplied parameters. Examples of the register file and memfiles can be found under the source folder