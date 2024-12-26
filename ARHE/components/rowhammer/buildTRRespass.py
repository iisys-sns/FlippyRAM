import sys
import os
import argparse

script_dir = os.path.dirname(os.path.abspath(__file__))
injected_functions = os.path.join(script_dir, '../../data/injected-functions.txt')
cppSnippet = os.path.join(script_dir, '../../data/tmp/generated_snippet_hammersuite.c')
fNum_path = os.path.join(script_dir, '../../data/functions-num.txt')

parser = argparse.ArgumentParser()
parser.add_argument('-d', '--default', action='store_true')
args = parser.parse_args()

with open(fNum_path, 'r') as file:
    fNum = file.read().strip()

fNum = int (fNum)

if (fNum == 5):
    row_fn = "0xffffc0000"
    def_fns = "0x2040,0x20000,0x44000,0x88000,0x110000"
elif (fNum == 6):
    row_fn = "0xffff80000"
    def_fns = "0x4080,0x88000,0x110000,0x220000,0x440000,0x4b300"
elif (fNum == 4):
    row_fn = "0xffffe0000"
    def_fns = "0x90000,0x48000,0x24000,0x2040"
else:
    print("The number of banks is not defined for Hammersuite!")
    sys.exit(1)

s1 = "DRAMLayout      g_mem_layout = {"
s2 = "{"
s3 = "{"
if args.default:
    s4=def_fns
else:
    with open(injected_functions, "r") as file:
        injected_fns = file.read().strip()

    num_list = eval(injected_fns)
    hex_list = [hex(num) for num in num_list]
    fb = True
    s4=""
    for x in hex_list:
        if fb == True:
            s4=s4 + x
        else:
            s4=s4 + "," + x
        fb = False
s5 = "}"
s6 = ", " + str(fNum) + "}" 
s7 = ", " + row_fn + ", ROW_SIZE-1};"
snippet = s1 + s2 + s3 + s4 + s5 + s6 + s7

with open(cppSnippet, "w") as file:
    file.write(snippet)
