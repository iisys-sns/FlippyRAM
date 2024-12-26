import sys
import os
import numpy as np
import pprint as pp
from datetime import datetime

class BinInt(int):
    def __repr__(s):
        return s.__str__()

    def __str__(s):
        return f"{s:#032b}"


class DRAMFunctions():

    def __init__(self, bank_fns, row_fn, col_fn, num_channels, num_dimms, num_ranks, num_banks):
        def to_binary_array(v):
            vals = []
            for x in range(30):
                if (v >> x) & 1:
                    vals.append(1 << x)
            return list(reversed(vals))

        def gen_mask(v):
            len_mask = bin(v).count("1")
            mask = (1 << len_mask)-1
            return (len_mask, mask)

        bank_mask = (1 << len(bank_fns))-1
        row_arr = to_binary_array(row_fn)
        len_row_mask, row_mask = gen_mask(row_fn)
        col_arr = to_binary_array(col_fn)
        len_col_mask, col_mask = gen_mask(col_fn)

        self.row_arr = row_arr
        self.col_arr = col_arr
        self.bank_arr = bank_fns
        self.row_shift = 0
        self.col_shift = len_row_mask
        self.bank_shift = len_row_mask + len_col_mask
        self.row_mask = BinInt(row_mask)
        self.col_mask = BinInt(col_mask)
        self.bank_mask = BinInt(bank_mask)
        self.num_channels = num_channels
        self.num_dimms = num_dimms
        self.num_ranks = num_ranks
        self.num_banks = num_banks

    def to_dram_mtx(self):
        mtx = self.bank_arr + self.col_arr + self.row_arr
        return list(map(lambda v: BinInt(v), mtx))

    def to_addr_mtx(self):
        dram_mtx = self.to_dram_mtx()
        mtx = np.array([list(map(int, list(f"{x:030b}"))) for x in dram_mtx])
        assert mtx.shape == (30, 30)
        inv_mtx = list(map(abs, np.linalg.inv(mtx).astype('int64')))
        inv_arr = []
        for i in range(len(inv_mtx)):
            inv_arr.append(BinInt("0b" + "".join(map(str, inv_mtx[i])), 2))
        return inv_arr

    def __repr__(self):
        dram_mtx = self.to_dram_mtx()
        addr_mtx = self.to_addr_mtx()
        sstr = "struct MemConfiguration dram_cfg = {\n"
        sstr += f"    .IDENTIFIER = (CHANS({self.num_channels}UL) | DIMMS({self.num_dimms}UL) | RANKS({self.num_ranks}UL) | BANKS({self.num_banks}UL)),\n"
        sstr += "    .BK_SHIFT = {0},\n".format(self.bank_shift)
        sstr += "    .BK_MASK = ({0}),\n".format(self.bank_mask)
        sstr += "    .ROW_SHIFT = {0},\n".format(self.row_shift)
        sstr += "    .ROW_MASK = ({0}),\n".format(self.row_mask)
        sstr += "    .COL_SHIFT = {0},\n".format(self.col_shift)
        sstr += "    .COL_MASK = ({0}),\n".format(self.col_mask)
        
        str_mtx = pp.pformat(dram_mtx, indent=8)
        trans_tab = str_mtx.maketrans('[]', '{}')
        str_mtx = str_mtx.translate(trans_tab)
        str_mtx = str_mtx.replace("{", "{          \n ")
        str_mtx = str_mtx.replace("}", "\n    },")
        sstr += f"    .DRAM_MTX = {str_mtx}\n"
        
        str_mtx = pp.pformat(addr_mtx, indent=8)
        trans_tab = str_mtx.maketrans('[]', '{}')
        str_mtx = str_mtx.translate(trans_tab)
        str_mtx = str_mtx.replace("{", "{          \n ")
        str_mtx = str_mtx.replace("}", "\n    }")
        sstr += f"    .ADDR_MTX = {str_mtx}"
        
        sstr += "\n};\n"

        return sstr


# SNS Implementation
script_dir = os.path.dirname(os.path.abspath(__file__))
injected_functions = os.path.join(script_dir, '../../data/injected-functions.txt')
headerSnippet = os.path.join(script_dir, '../../data/tmp/generated_snippet_rowpress.h')
addrFunctions = os.path.join(script_dir, '../../data/final-functions.txt')
amdreFunctions = os.path.join(script_dir, '../../data/amdre-functions.txt')
trrespassFunctions = os.path.join(script_dir, '../../data/trrespass-functions.txt')
dareFunctions = os.path.join(script_dir, '../../data/dare-functions.txt')
dramdigFunctions = os.path.join(script_dir, '../../data/dramdig-functions.txt')
banksPath = os.path.join(script_dir, '../../data/logs/blacksmithBanks-1.log')
ranksPath = os.path.join(script_dir, '../../data/logs/blacksmithRanks-1.log')
fNum_path = os.path.join(script_dir, '../../data/functions-num.txt')
channels_path = os.path.join(script_dir, '../../data/sys-info/channels.txt')
dimmNum_path = os.path.join(script_dir, '../../data/sys-info/dimm-numbers.txt')
threshold_path = os.path.join(script_dir, '../../data/threshold.txt')
errPath = os.path.join(script_dir, '../../data/errors/buildRowpress.txt')

with open(ranksPath, 'r') as file:
    num_ranks = file.read().strip()

with open(banksPath, 'r') as file:
    num_banks = file.read().strip()

with open(fNum_path, 'r') as file:
    fNum = file.read().strip()

with open(dimmNum_path, 'r') as file:
    num_dimms = file.read().strip()

with open(channels_path, 'r') as file:
    num_channels = file.read().strip()

with open(threshold_path, 'r') as file:
    threshold = file.read().strip()

threshold = int (threshold)
fNum = int (fNum)
num_ranks = int (num_ranks)
num_banks = int (num_banks)
num_dimms = int (num_dimms)
num_channels = int (num_channels)



dram_fns = []
amdre_fns = []
trrespass_fns = []
dare_fns = []
dramdig_fns = []
with open(addrFunctions, 'r') as f:
    addresses_str = f.read().strip()
    addresses_list = addresses_str.split(',')
    if addresses_str:
        for addr in addresses_list:
            addr_int = int(addr, 16)
            dram_fns.append(addr_int)

with open(amdreFunctions, 'r') as f:
    addresses_str = f.read().strip()
    addresses_list = addresses_str.split(',')
    if addresses_str:
        for addr in addresses_list:
            addr_int = int(addr, 16)
            amdre_fns.append(addr_int)

with open(trrespassFunctions, 'r') as f:
    addresses_str = f.read().strip()
    addresses_list = addresses_str.split(',')
    if addresses_str:
        for addr in addresses_list:
            addr_int = int(addr, 16)
            trrespass_fns.append(addr_int)

try:
    with open(dareFunctions, 'r') as f:
        addresses_str = f.read().strip()
        addresses_list = addresses_str.split(',')
        if addresses_str:
            for addr in addresses_list:
                addr_int = int(addr, 16)
                dare_fns.append(addr_int)
except Exception as e:
    dare_fns = [2134]
    with open(errPath, "a") as log_file:
        log_file.write(f"Dare ERROR: {e}\n")

try:
    with open(dramdigFunctions, 'r') as f:
        addresses_str = f.read().strip()
        addresses_list = addresses_str.split(',')
        if addresses_str:
            for addr in addresses_list:
                addr_int = int(addr, 16)
                dramdig_fns.append(addr_int)
except Exception as e:
    dramdig_fns = [2134]
    with open(errPath, "a") as log_file:
        log_file.write(f"DRAMDig ERROR: {e}\n")

amdre_fns_num = len(amdre_fns)
trrespass_fns_num = len(trrespass_fns)
dare_fns_num = len(dare_fns)
dramdig_fns_num = len(dare_fns)

# Note that def_fns (Default Functions are defined if the injection fails for any reason, those functions could still be used for Rowpress)
if (fNum == 5):
    row_fn = 0x3ffc0000
    def_fns = [0x2040,0x20000,0x44000,0x88000,0x110000]
elif (fNum == 6):
    row_fn = 0x3ff80000
    def_fns = [0x4080, 0x88000, 0x110000, 0x220000, 0x440000, 0x4b300]
elif (fNum == 4):
    row_fn = 0x3ffe0000
    def_fns = [0x90000,0x48000,0x24000,0x2000]
elif (fNum == 7):
    row_fn = 0x3ff00000
elif (fNum == 3):
    row_fn = 0x3fff0000
else:
    print("The number of banks is not correct!")
    sys.exit(1)
    
col_fn = 8192 - 1

try:
    res = DRAMFunctions(dram_fns, row_fn, col_fn, num_channels, num_dimms, num_ranks, num_banks)
    with open(headerSnippet, "w") as file:
        file.write(repr(res))
    # Just to save the injected functions
    with open(injected_functions, "w") as file:
        file.write(str(dram_fns))
except:
    # error report
    with open(errPath, "a") as file:
        now = datetime.now()
        dt_string = now.strftime("%d-%m-%Y %H:%M:%S")
        errString = f"[{dt_string}] Error Injecting Drama-Verify Functions: " + str(dram_fns)
        file.write(errString + "\n")
    print("Python: The functions seem to be wrong. Switching to AMDRE functions ...")
    try:
        if fNum == amdre_fns_num:
            res = DRAMFunctions(amdre_fns, row_fn, col_fn, num_channels, num_dimms, num_ranks, num_banks)
            with open(headerSnippet, "w") as file:
                file.write(repr(res))
            # Just to save the injected functions
            with open(injected_functions, "w") as file:
                file.write(str(amdre_fns))
        else:
            raise ValueError("AMDRE functions number is not correct!")
    except:
        # error report
        with open(errPath, "a") as file:
            now = datetime.now()
            dt_string = now.strftime("%d-%m-%Y %H:%M:%S")
            errString = f"[{dt_string}] Error Injecting AMDRE Functions: " + str(amdre_fns)
            file.write(errString + "\n")
        print("Python: The functions seem to be wrong. Switching to TRRespass functions ...")
        try:
            if fNum == trrespass_fns_num:
                res = DRAMFunctions(trrespass_fns, row_fn, col_fn, num_channels, num_dimms, num_ranks, num_banks)
                with open(headerSnippet, "w") as file:
                    file.write(repr(res))
                # Just to save the injected functions
                with open(injected_functions, "w") as file:
                    file.write(str(trrespass_fns))
            else:
                raise ValueError("TRRespass functions number is not correct!")
        except:
            # error report
            with open(errPath, "a") as file:
                now = datetime.now()
                dt_string = now.strftime("%d-%m-%Y %H:%M:%S")
                errString = f"[{dt_string}] Error Injecting TRRespass Functions: " + str(trrespass_fns)
                file.write(errString + "\n")
            print("Python: The functions seem to be wrong. Switching to Dare functions ...")
            try:
                if fNum == dare_fns_num:
                    res = DRAMFunctions(dare_fns, row_fn, col_fn, num_channels, num_dimms, num_ranks, num_banks)
                    with open(headerSnippet, "w") as file:
                        file.write(repr(res))
                    # Just to save the injected functions
                    with open(injected_functions, "w") as file:
                        file.write(str(dare_fns))
                else:
                    raise ValueError("Dare functions number is not correct!")
            except:
                # error report
                with open(errPath, "a") as file:
                    now = datetime.now()
                    dt_string = now.strftime("%d-%m-%Y %H:%M:%S")
                    errString = f"[{dt_string}] Error Injecting Dare Functions: " + str(dare_fns)
                    file.write(errString + "\n")
                print("Python: The functions seem to be wrong. Switching to DRAMDig functions ...")
                try:
                    if fNum == dramdig_fns_num:
                        res = DRAMFunctions(dramdig_fns, row_fn, col_fn, num_channels, num_dimms, num_ranks, num_banks)
                        with open(headerSnippet, "w") as file:
                            file.write(repr(res))
                        # Just to save the injected functions
                        with open(injected_functions, "w") as file:
                            file.write(str(dramdig_fns))
                    else:
                        raise ValueError("DRAMDig functions number is not correct!")
                except:
                    with open(errPath, "a") as file:
                        now = datetime.now()
                        dt_string = now.strftime("%d-%m-%Y %H:%M:%S")
                        errString = f"[{dt_string}] Error Injecting DRAMDig Functions: " + str(dare_fns)
                        file.write(errString + "\n")
                    print("Python: The functions seem to be wrong. Switching to DEFAULT functions ...")
                    res = DRAMFunctions(def_fns, row_fn, col_fn, num_channels, num_dimms, num_ranks, num_banks)
                    with open(headerSnippet, "w") as file:
                        file.write(repr(res))
                    # Just to save the injected functions
                    with open(injected_functions, "w") as file:
                        file.write(str(def_fns))