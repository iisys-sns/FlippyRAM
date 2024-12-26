# AMDRE
`amdre` is a reverse-engineering tool for addressing functions on AMD systems.
It does work on Intel systems as well, but the focus is on systems with AMD
CPUs since these systems are, to the best of our knowledge, currently not
supported by other proof-of-concepts.

See our paper [Reverse-Engineering Bank Addressing Functions on AMD
CPUs](https://dramsec.ethz.ch/papers/revengamd.pdf) for a detailed description
of the approach and results of this tool. Please cite it as following:

```
@inproceedings{heckel:amdre:2023,
  title     = {{Reverse-Engineering Bank Addressing Functions on AMD CPUs}},
  author    = {Heckel, Martin and Adamsky, Florian},
  booktitle = {Proceedings of the the 3rd Workshop on DRAM Security (DRAMSec 2023), co-located with ISCA},
  date      = {2023},
}
```

## Usage
The tool `amdre` should be executed as root. Run the following command for a
list of supported command-line parameters:
```
./bin/amdre -h
```
Many of these parameters have default values as shown in the help page.
Depending on the system, it might be required to adjust some of the default
values. The following list gives an overview which parameters can be adjusted
when one of the steps fails. It should be noted that the steps depend on each
other: When one step fails, the following steps will fail (or not yield
correct results) as well.

* Threshold Calculation  
The threshold between row hit and row conflict is measured automatically. When
that step fails, it can be specified manually using the
`-T, --row-conflict-threshold=TIME` command line argument.

* Determination of the number of DRAM banks  
Based on the threshold, a number of initial THPs (1 by default) with an initial
block size (4096B by default) is grouped by access time to represent DRAM banks.
When the number of banks detected does not match the number of banks in the
system, it is possible to add more initial THPs (`-i, --initial-thps=NUMBER`)
or modify the initial block size (`-b, --initial-block-size=SIZE`).

* Determination of the block size  
In the next step, the block size (e.g., how many addresses are always following
each other within the same DRAM bank) is detected. If the automatic detection
fails, it is possible to manually specify the block size
(`-B, --block-size=SIZE`).

* Derivation of addressing functions  
Afterwards, the addressing functions are derived based on the addresses grouped
before. It should be noted that `amdre` can only derive linear addressing
functions, e.g. on systems where the number of DRAM banks is a power of two.
When addressing functions are not detected, the number of maximum bits (7 by
default) might be too low. It can be modified with the command line parameter
`-x, --max-mask-bits=NUMBER`.

It might be possible that the first bits of address masks are not correctly
detected. That can be solved by grouping more additional THPs using the command
line option `-a, --additional-thps=NUMBER`.

## Example
The following example shows the output of the tool running on a system with an
AMD Ryzen 9 3900X and one DIMM as described in the results Section of our paper.

```
sudo ./bin/amdre -d --max-mask-bits=9
```

```
 INFO      2023-06-21 10:26:34: Measured theshold: 820
 INFO      2023-06-21 10:26:34: Filling initial bank groups...
 DEBUG     2023-06-21 10:26:34: Adding THP 1 of 1 to the bank group.
 DEBUG     2023-06-21 10:26:39: Added all addresses of the THP to the banks. There are 79 groups now.
 INFO      2023-06-21 10:26:39: Regrouping bank groups until they look plausible (number of banks should be a power of 2)...
 DEBUG     2023-06-21 10:26:50: Regrouping addresses (try 2).
 DEBUG     2023-06-21 10:26:50: Regrouping done. There are 33 groups now.
 DEBUG     2023-06-21 10:26:57: Regrouping done. There are 32 groups now.
 INFO      2023-06-21 10:26:57: Assuming 32 banks.
 INFO      2023-06-21 10:26:57: Detecting block size...
 DEBUG     2023-06-21 10:29:54: Current block size is bigger or equal to the actual block size. Reducing block size to 64.
 DEBUG     2023-06-21 10:27:02: Expansion of address groups done. Added 512 new addresses with 0 errors..
 DEBUG     2023-06-21 10:27:14: Expansion of address groups done. Added 1024 new addresses with 0 errors..
 DEBUG     2023-06-21 10:27:36: Expansion of address groups done. Added 2048 new addresses with 0 errors..
 DEBUG     2023-06-21 10:28:21: Expansion of address groups done. Added 4096 new addresses with 0 errors..
 DEBUG     2023-06-21 10:29:54: Expansion of address groups done. Added 8192 new addresses with 0 errors..
 DEBUG     2023-06-21 10:33:06: Expansion of address groups done. Added 16384 new addresses with 1 errors..
 INFO      2023-06-21 10:33:06: Assuming a block size of 64 bytes
 DEBUG     2023-06-21 10:33:06: Adding more addresses to the group.
 INFO      2023-06-21 10:33:06: Additional addresses were added to groups. A total of 0 pages with 0 errors.
 INFO      2023-06-21 10:33:06: Calculating address functions. This may take a while.
 DEBUG     2023-06-21 12:39:24: Found 16 non-unified address functions:
 DEBUG     2023-06-21 12:39:24: 	0x4000
 DEBUG     2023-06-21 12:39:24: 	0x48000
 DEBUG     2023-06-21 12:39:24: 	0x90000
 DEBUG     2023-06-21 12:39:24: 	0x94000
 DEBUG     2023-06-21 12:39:24: 	0x4c000
 DEBUG     2023-06-21 12:39:24: 	0xd8000
 DEBUG     2023-06-21 12:39:24: 	0x1e0000
 DEBUG     2023-06-21 12:39:24: 	0x170000
 DEBUG     2023-06-21 12:39:24: 	0x1a8000
 DEBUG     2023-06-21 12:39:24: 	0x138000
 DEBUG     2023-06-21 12:39:24: 	0x174000
 DEBUG     2023-06-21 12:39:24: 	0x13c000
 DEBUG     2023-06-21 12:39:24: 	0x1e4000
 DEBUG     2023-06-21 12:39:24: 	0x1ac000
 DEBUG     2023-06-21 12:39:24: 	0xdc000
 DEBUG     2023-06-21 12:39:24: 	0x103fc0
 DEBUG     2023-06-21 12:39:24: Masks are orthogonal.
 DEBUG     2023-06-21 12:39:24: Mask 0x4000: 0 1 0 1 1 0 0 1 1 0 1 1 1 1 0 0 1 0 0 0 1 0 0 1 1 0 0 1 1 0 1 0
 DEBUG     2023-06-21 12:39:24: Mask 0x48000: 1 1 0 0 0 1 1 1 1 0 0 0 1 1 0 0 0 1 0 1 1 0 0 0 0 1 1 1 1 0 0 1
 DEBUG     2023-06-21 12:39:24: Mask 0x90000: 0 0 1 1 0 0 0 0 0 1 1 1 1 1 0 0 0 1 1 0 0 0 0 0 0 1 1 1 1 1 1 1
 DEBUG     2023-06-21 12:39:24: Mask 0x103fc0: 0 0 0 1 1 0 1 0 1 0 1 0 0 1 0 1 0 0 1 1 1 1 0 1 0 1 0 1 0 1 0 1
 DEBUG     2023-06-21 12:39:24: Mask 0x138000: 1 1 1 1 1 0 0 0 0 0 0 0 1 1 1 1 1 1 0 1 1 0 0 0 0 0 0 0 0 1 1 1
 INFO      2023-06-21 12:39:24: Found 5 address functions.
 INFO      2023-06-21 12:39:24: Address Function: 0x4000 seems to be valid.
 INFO      2023-06-21 12:39:24: Address Function: 0x48000 seems to be valid.
 INFO      2023-06-21 12:39:24: Address Function: 0x90000 seems to be valid.
 INFO      2023-06-21 12:39:24: Address Function: 0x103fc0 seems to be valid.
 INFO      2023-06-21 12:39:24: Address Function: 0x138000 seems to be valid.
 INFO      2023-06-21 12:39:24: Address functions calculated successfully.
```

The DRAM addressing function identified by `amdre` are:
* 0x4000
* 0x48000
* 0x90000
* 0x103fc0
* 0x138000
