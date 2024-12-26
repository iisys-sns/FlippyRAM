# DRAMDig: A Knowledge-assisted Tool to Uncover DRAM Address Mapping
DRAM address mapping is critial for Rowhammer attack detection and mitigations. DRAMDig is a knowledge-assisted tool that takes domain knowledge into consideration to efficiently and deterministically uncover the DRAM address mappings on any Intel-based machines with Non-ECC DDR3/DDR4 DRAM modules. It deterministically reverse-engineered DRAM address mappings on all the test machines with only 7.8 minutes on average, 69 seconds at least from our experiments.

DRAMDig can help users better understand DRAM address mapping and evaluate the impact of rowhammer threats. Based on the uncovered mappings, double-sided rowhammer tests can be performed and our experimental results showed that significantly more bit flips were induced than previous works. The paper describing more details will appear at DAC 2020 and can be found [here](https://arxiv.org/abs/2004.02354). 

## Getting Started

### Prerequisites 
You need install i2c-tools and load the modules before running DRAMDig.

#### Install i2c-tools
```
wget https://fossies.org/linux/misc/i2c-tools-4.1.tar.gz
tar xzvf i2c-tools-4.1.tar.gz
cd i2c-tools-4.1
make
sudo make install
```

For DDR4 settings, you also need to compile and install ee1004 kernel module.

```
wget http://jdelvare.nerim.net/devel/lm-sensors/drivers/ee1004/Makefile
wget http://jdelvare.nerim.net/devel/lm-sensors/drivers/ee1004/ee1004.c
make 
sudo make install
```

#### Load i2c-tools modules
Run the following commands to load the modules.

```
sudo modprobe eeprom
sudo modprobe i2c-i801
sudo modprobe i2c-dev
```

If you are testing DDR4 DRAM, you also need to load `ee1004`.

```
sudo modprobe ee1004
```

You can verity the success of the above procedure by running `decode-dimms`. If you obtained the similar result as the following, that means i2c-tools are installed properly.

```
Memory Serial Presence Detect Decoder
By Philip Edelbrock, Christian Zuckschwerdt, Burkart Lingner,
Jean Delvare, Trent Piepho and others

Decoding EEPROM: /sys/bus/i2c/drivers/eeprom/3-0052
Guessing DIMM is in                              bank 3

---=== Memory Characteristics ===---
Maximum module speed                             2400 MHz (PC4-19200)
Size                                             16384 MB
Banks x Rows x Columns x Bits                    16 x 16 x 10 x 64
SDRAM Device Width                               8 bits
Ranks                                            2
Rank Mix                                         Symmetrical
AA-RCD-RP-RAS (cycles)                           17-17-17-39
Supported CAS Latencies                          18T, 17T, 16T, 15T, 14T, 13T, 12T, 11T, 10T
......

```

### Building DRAMDig
Type `make` to build DRAMDig.

```
make
```

## Running the tests
Uncovering DRAM address mappings with DRAMDig can be accomplished within two steps. We give an example test settings at first. All the outputs shown in the following steps were generated under this setting.

### Example Test Setting

- CPU: Intel Core i7-6700K (SkyLake)
- Motherboard: Gigabyte Z170N
- Memory: Samsung 16G DDR4 (Non-ECC)
- DRAM Setting: 1 channel, 1 DIMM/channel, 2 ranks/DIMM; 32 banks in total
- Ubuntu 18.04 LTS with its default Linux kernel

### Step 1
Firstly run the script `detect_setups.py`.

```
sudo python3 detect_setups.py
```

The script will gather symtem information such as physical memory size of current machine and the DRAM setting details including total number of banks, number of row bits and column bits, which are obtained by invoking decode-dimms from the script. The finall output is the config.ini and it looks like as follows. (BTW, if the number of bank is 0, it is probably caused by misconfiguration of i2c-tools, you can restart the machine and do it again, which is a rare case.)

```
// config.ini:
MAX_XOR_BITS 7
NUM_OF_READ 2000
BANKS_NUMBER_TOTAL 32
SPEC_ROW_BITS_NUMBER 16
SPEC_COLUMN_BITS_NUMBER 13
AVAILABLELENGTH 34
```

`MAX_XOR_BITS` is the maximum number of bits that a bank address function can have. `NUM_OF_READ` is the number of memory access to the hammered address in a round. `BANKS_NUMBER_TOTAL` represents the total number of the banks, while `SPEC_ROW_BITS_NUMBER` and `SPEC_COLUMN_BITS_NUMBER` are physical-address bit numbers that index rows and columns. `AVAILABLELENGTH` represents the maximum number of bits in a physical-address. 

You can also edit the `MAX_XOR_BITS` and `NUM_OF_READ` to your own values to best suit for your testing setups. By default, we asumme that `MAX_XOR_BITS` to be 7 and set `NUM_OF_READ` to be 2000.

### Step 2
Run `run_dramdig.sh` to uncover DRAM address mappings.

```
sudo ./run_dramdig.sh | tee 1.log
```

You can see the DRAM address mapping eventually uncovered when the above command accomplished. The following was an example output.

```
// 1.log:
......
=== bank addr funs (5): ===
17, 20,
16, 19,
15, 18,
7, 14,
8, 9, 12, 13, 18, 19,
 === row bits: (16) ===
18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33,
 === column bits: (13)===
0, 1, 2, 3, 4, 5, 6, 7, 9, 10, 11, 12, 13,
```

5 bank address functions were uncovered, which are (8, 9, 12, 13, 18, 19), (7, 14), (15, 18), (16, 19), (17, 20). The row bits number is 16, which are from bit 18 to bit 33, and the column bits number is 13, which are 0 ~ 7 and 9 ~ 13.

## Contributing
Contrbutions are welcome. Feel free to send us pull requests on the GitHub. If you have questions, you can also talk to our maintainers for help.

## Acknowledgments
We borrowed some code from DRAMA [1] and Yuan Xiao et al. work [2] when developing DRAMDig. Many thanks to them all.

[1] https://github.com/IAIK/drama.

[2] One Bit Flips, One Cloud Flops: Cross-VM Row Hammer Attacks and Privilege Escalation. Yuan Xiao, Xiaokuan Zhang, Yinqian Zhang, Mircea-Radu Teodorescu. Usenix Sec'16.



## Discussion
Feel free to submit Github issues, pull requests, or contact the following maintainers.

- Zhi Zhang: [Github](https://github.com/henryzhi), [Email](mailto:zhi.zhang@uwa.edu.au)
- Wei He: [Github](https://github.com/Emoth97), [Email](mailto:hewei@iie.ac.cn)
- Yueqiang Cheng: [Github](https://github.com/strongerwill), [Email](mailto:yueqiang.cheng@nio.io)
