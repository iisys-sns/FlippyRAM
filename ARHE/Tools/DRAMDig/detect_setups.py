#!/usr/bin/python3

"""
   Copyright (c) 2018 Baidu, Inc. All Rights Reserved.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
"""

import os
import sys
import re
import math
import subprocess

def write_file(fname, lines):
    """Write lines info file with fname.

    Args:
        fname: the file name of the written file.
        lines: the lines that written to the file
    Returns: No return values
    """
    with open(fname, "w") as f:
        f.writelines(lines)


def retrive_physaddr_bits_num():
    """Invoke `dmidecode -t memory` command to get physical-address bit number.

    Returns:
        The number of physical-address bits.
    """
    r = subprocess.check_output(['dmidecode', '-t', 'memory'])
    dmi = r.decode('ascii')
    write_file("./dmidecode.out", dmi)
    dmi = dmi.split('\n')
    total_sz = 0
    p = re.compile(r"^\s*Size: (\d+) GB")
    for l in dmi:
        m = re.match(p, l.strip())
        if m:
            print("Size: {} GB".format(m.group(1)))
            total_sz += int(m.group(1)) * 1024
    
    print("total phy mem size: {} MB, bit count: {}".format(total_sz, 20 + int(math.log(total_sz, 2))))
    return 20 + int(math.log(total_sz, 2))


# return: [(start, end), (start, end), ...]
def retrive_memory_characteristics(lines):
    """Parse out the positions of Memory Characteristics in the lines.

    Args:
        lines: the content in lines
    Returns:
        A list of (start, end). 'start' specifies the starting line number and 'end' specifies the ending line number.
    """
    ret = []
    start, end = 0, 0
    for i in range(0, len(lines)):
        if lines[i].find("---=== Memory Characteristics ===---") != -1:
            start = i 
        if start != 0 and i > start and lines[i].find("---===") != -1:
            end = i
            ret.append((start, end))
            start = end = 0
    return ret


def retrive_banks_num():
    """Retrieve the total bank numbers, row bit and column bit numbers of current machine setting by invoking 'decode-dimms'.

    Args: 
        No Args
    Returns:
        A tuple with format of (#bank, #rowbits, #columnbits)
    """
    #if subprocess.run(['modprobe', 'eeprom']).returncode != 0:
    #    print("[-] modprobe eeprom error. abort")
    #    exit(-1)
    
    if subprocess.run(['modprobe', 'i2c-i801']).returncode != 0:
        print("[-] modprobe i2c-i801 error. abort")
        exit(-1)

    r = subprocess.check_output(['/bin/bash', 'decode-dimms.sh'])
    lines = r.decode('ascii')
    write_file("./decode-dimms.out", lines)
    lines = lines.split('\n')
    
    ranges = retrive_memory_characteristics(lines)
    print("mem characteristics ranges: {}".format(ranges))
    banks_num, rowbits_num, colnbits_num = 0, 0, 0
    p1 = re.compile(r"^Banks x Rows x Columns x Bits\s+(\d+) x (\d+) x (\d+) x (\d+)$")
    p2 = re.compile(r"^Ranks\s+(\d+)$")
    for (start, end) in ranges:
        banks, ranks, rowbits, colnbits = 0, 0, 0, 0
        for i in range(start, end):
            m1 = re.match(p1, lines[i])
            m2 = re.match(p2, lines[i])
            if m1:
                banks = int(m1.group(1))
                rowbits = int(m1.group(2))
                colnbits = int(m1.group(3))
            if m2:
                ranks = int(m2.group(1))
        banks_num += banks * ranks
        if rowbits_num != 0 and rowbits_num != rowbits:
            print("[!] NOTICE: you have different brand mem chips in your machine!")
            print("[!] first detect: row bits: {}, now detect: {}".format(rowbits_num, rowbits))
        else:
            rowbits_num = rowbits
            colnbits_num = colnbits + 3
     
    print("total banks count: {}, rowbits num: {}".format(banks_num, rowbits_num))
    return (banks_num, rowbits_num, colnbits_num)


def retrive_others():
    """Setup MAX_XOR_BITS and NUM_OF_READ with the default value.
    
    Args:
        No Args.
    Returns:
        No Returns.
    """
    MAX_XOR_BITS, NUM_OF_READ = 7, 2000
    print("max_xor_bits is set to {} by default.\n".format(MAX_XOR_BITS))
    print("num_of_read is set to {} by default.\n".format(NUM_OF_READ))
    return (MAX_XOR_BITS, NUM_OF_READ)


def main():
    """Gather necessary knowledge and write them to config.ini.
    
    Args:
        No Args.
    Returns:
        No Returns.
    """
    bit_num = retrive_physaddr_bits_num()
    bank_num, rowbit_num, colnbit_num = retrive_banks_num()
    max_xor_bits, num_of_read = retrive_others()
    with open('./config.ini', 'w') as f:
        f.write("MAX_XOR_BITS {}\n".format(max_xor_bits))
        f.write("NUM_OF_READ {}\n".format(num_of_read))
        f.write("BANKS_NUMBER_TOTAL {}\n".format(bank_num))
        f.write("SPEC_ROW_BITS_NUMBER {}\n".format(rowbit_num))
        f.write("SPEC_COLUMN_BITS_NUMBER {}\n".format(colnbit_num))
        f.write("AVAILABLELENGTH {}\n".format(bit_num))


if __name__ == '__main__':
    main()
