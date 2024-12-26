# HammerISO
HammerISO is an ISO image that can be used for automated Rowhammer testing of
different systems. In the end of the experiment, a brief overview of the number
of bit flips found by different tools is shown.

In addition to the live system, the ISO image contains a second partition that
is automatically resized to the maximum size at the first boot of the image.
Following boots will not resize the partition. The logs written by the different
tools are compressed and stored in that partition so they can be analyzed
afterwards.

Additionally, the test setup asks the user to upload the results to a server.
It basically takes the compressed results and use `curl` to upload them to the
server currently running at [flippyr.am](https://flippyr.am).

## Building
You can build the current version of the ISO image by running the following
command:
```bash
make
```

Note that, since the ISO building process is based on ArchISO, this requires
ArchLinux in order to build the ISO image.

After building the ISO image is done, it will create a new file `hammeriso.iso`
that can be published and flashed afterwards.

## Flashing
When it is required to flash the image to multiple thumb drives, they can be
connected to the system at the same time. Afterwards, the image can be flashed
to all to them by running the following command:
```bash
sudo sh utilities/flash.sh <device-size> <path/to/ISO/image.iso>
```

Thereby, `<device-size>` specifies the storage size of the thumb drive. Be
aware, this step will flash the image to **ALL CONNECTED USB DEVICE THAT HAVE
THE SPECIFIED SIZE**, so be sure to not have other USB devices with the same
size connected to the system.

The parameter `<path/to/ISO/image.iso>` specifies the path to the ISO image.

For example, the following command can be used to flash to all USB devices with
a size of `57.8G`. **BEWARE: THIS WILL OVERWRITE ALL DEVICES OF THAT SIZE!**
```bash
sudo sh utilities/flash.sh 57.8G hammeriso.iso
```

## Add a new Rowhammer Tool
In general, you should run you tool manually and verify that it works before
automating it. After a first test run on a system was successful, use the
following steps to add you tool to the ISO image.

### Create a new Branch
First, you should create a new branch based on `ARHE-v0.4`. You should implement
everything on this branch and merge to `ARHE-v0.4` when everything works. This
ensures that the version in `ARHE-v0.4` is runnable all the time and can be
used for continuous beta testing.

### Add the source code
Download and clear up the source code of the PoC (e.g., removing the `.git`
directory, unneeded binary files, etc.). Copy everything that is needed to
compile the PoC to the following directory:
```bash
ARHE/Tools/<your-tool>/
```

### Add required dependencies
Next, it might be required to install some packages to the ISO image for the
tool to compile and/or run. Adjust the following file and add the required
dependencies if they are not already specified:
```bash
ISO/packages.x86_64
```

Additionally, you should add your dependency to the Dockerfile so the docker
container runs with your tool as well. Edit the following file to add your
dependencies:
```bash
ARHE/Dockerfile
```

### Create a script to automatically build the tool
Rowhammer tools are compiled directly before they are executed. You should
create a script that performs required adjustments (e.g. insert addressing
functions based on the ones identified before) and builds the tool afterwards.
Follow this naming scheme to keep some structure in the project:
```bash
ARHE/components/rowhammer/build<your-tool>.sh
```

* **Hint**: You can use the `HammerTool` build script as template:
```bash
ARHE/components/rowhammer/buildHammerTool.sh
```

### Create a script to execute the tool
Similar to the script that builds the tool described before, there should be
another script that runs the tool at:
```bash
ARHE/components/rowhammer/run<your-tool>.sh
```

Please keep the following constraints in mind while adding a new script:
* The output should be written to a log file and it is ensured that the size of the log file does not exceed a specific size (e.g., avoid disk space exhaustion)
* Respect the runtime limits submitted by the tool (e.g., avoid the tool running longer than specified, optionally restart if time is left (or just return without taking all available runtime))
* Ensure that the tool does not continue to run after the script returns (e.g., do not interfere with other experiments that are started after your experiment)
* Ensure that the tool does not trigger severe crashes (it is ok when the tool crashes on some systems, but it should not crash the entire system)

* **Hint**: You can use the `HammerTool` run script as a template:
```bash
ARHE/components/rowhammer/runHammerTool.sh
```

### Add the tool to the config file
Next, add you tool to the config file, so it can be enabled and disabled for
different runs. Edit the following file to do that:
```bash
ARHE/conf.cfg
```

Just add a line in the section `Tool Control` following the existing naming
scheme of:
* `ENABLE_ROWHAMMER_<your-tool>=1`

For testing, you can (and should) disable the other Rowhammer Tools by setting
their enabled flag to `0`. You can also disable some Reverse-Engineering tools
to not wait as long for addressing functions. Just select one
Reverse-Engineering tool that works on your system and disable everything else.

Additionally, you might want to adjust the default runtime to reduce the time
a single test is running. Just set `DEFAULT_RUNTIME` to the time in seconds the
experiment should run overall.

### Add the tool to the list of tools to disable when hugepage support fails (optional)
On some systems, allocation of a 1G hugepage might fail. In `ARHE/main.sh`,
there is a section which tries to get a 1G hugepage and disables all tools that
require a 1G hugepage if that fails.

If that is applicable for the tool, disable your tool after hugepage allocation
failed by adding a line similar to the following to the according section (just
search for `disableToolInConf` to find the section):
```bash
disableToolInConf "ENABLE_<your-tool-config-option>"
```

A similar approach is also applied in other cases, e.g., when reverse-engineering
of addressing functions fails. Just check `main.sh` for `disableToolInConf` to
identify these locations.

### Add the tool to the list of tools for counting the runtime
Edit `main.sh` and add your tool in the following format to the array in the
`countRemainingTools` function:
```bash
"$ENABLE_ROWHAMMER_<your-tool>"
```

### Add the tool to the list of tools to check if disabled
Edit `main.sh` and add your tool to the function `logDisabledTools`. Thereby,
the tool will be logged as disabled when the enabled flag in the config was not
set when executing the experiment.

### Execute the tool
Add the tool to `ARHE/main.sh`. Just add the following lines to the corresponding
Sections:
```bash
# <your-tool> RH Expoit
############ (new format, please use this for the tools you are adding)
adjustAvailableRuntime
runRowhammerTool "$ENABLE_ROWHAMMER_<your-tool>" "<your-tool>"
```

* **Hint**: You can use `HammerTool` as an example.

### Add the output of the tool to the ncurses results page
Next, you should add the results of the experiment you added to the ncurses
results page so the user sees the number of bit flips found by this experiment.
Just edit `ARHE/run.sh` and add the following lines at the according place (search
for `hammertoolBitFlips` to find the section):
```bash
<your-tool>BitFlips="0"
if [ -f "$root_path/data/tmp/<your-tool>BitFlipsNum.tmp" ]; then
	<your-tool>BitFlips=$(cat "$root_path/data/tmp/<your-tool>BitFlipsNum.tmp")
fi
```

Then, you should add a line showing the number of bit flips in the final results
dialog by adding the following line:
```bash
<your-tool> Bit Flips: \Z5\Zb$<your-tool>BitFlips\Zn\n\n\
```

### Testing and Merging
After you implemented the new experiment according to the steps described above,
you should perform some tests. You can either perform these tests yourself or
ask us to perform them (e.g. if you don't have sufficient time). The tests
should be performed in multiple stages:
* Deploy your current state to the system you want to test at
	* run `main.sh` first to see the command-line output of the scripts, tools, etc.
	* run `run.sh` after the tests with `main.sh` worked to test the UI
	* run `sh docker-run.sh` to test the docker container
* Build the ISO image by running `make`, flash it to a thumb drive and boot it
  on some systems

After all tests succeeded (command line version, UI, ISO image), you can merge
your branch back to `ARHE-v0.4` (or ask us to merge).

## List of information we collect (see source code at TODO for more details)
* In general, there are 6 types of information we collect
  * General execution information (e.g., which tools were executed, how long did they run, etc.)
  * System information that enable fingerprinting (e.g., serial numbers, etc.)
  * System information that do not enable fingerprinting (e.g., vendor, product name, etc.)
  * System information that were reverse-engineered (e.g., linear DRAM addressing functions, refresh rate, etc.)
  * Logs for debugging and classification of data sets (e.g., did the tool crash or just not find anything, etc.)
  * Logs of Bit Flip locations (e.g., physical addresses of victim rows and sometimes aggressor rows, etc.)
### System Information
* DIMM information based on SPD records of DIMMs (e.g. , Layout, Timings, Serial Numbers, etc.)
	* Output of `decode-dimms`
	* Output of `dmidecode --type memory`
* Information about the Mainboard (e.g., Manufacturer, Model, Version, Serial Number, etc.)
	* Output of `dmidecode --type baseboard`
* Information about the BIOS (e.g., Vendor, Version, ROM Size, Firmware Revision, etc.)
	* Output of `dmidecode --type bios`
* Information about the System (e.g., Manufacturer, Version, Serial Number, etc.)
	* Output of `dmidecode --type system`
* Information about the Chassis (e.g., Manufacturer, Type, Serial Number, etc.)
	* Output of `dmidecode --type chassis`
* Information about the CPU (e.g., Manufacturer, Model, Flags, Characteristics, Microcode, etc.)
	* Output of `dmidecode --type processor`
	* Output of `lscpu`
	* Output of `dmesg | grep "microcode"`
* Information about the Cache (e.g., Size, Cache Levels, Type, Associativity, etc.)
	* Output of `dmidecode --type cache`
* Information about peripheral connectors (e.g., USB, HDMI, Headphone/Microphone Jack, etc.)
	* Output of `dmidecode --type connector`
* Information about peripheral slots (e.g., SIM Card slots, etc.)
	* Output of `dmidecode --type slots`
### Build Output
* Build logs of the included tools
	* General System information
		* MeasureRefreshRate
	* Addressing Function Reverse-Engineering
		* AMDRE
		* DRAMDig
		* Drama
		* Dare
		* TRRespass RE
	* Linear DRAM Addressing Function Verification
		* Drama-Verify
	* Rowhammer Tools
		* Blacksmith
		* HammerSuite (Tool from the TRRespass Paper)
		* HammerTool
### Tool Output
* All tools implicit: Runtime of the tool
#### General System Information
* MeasureRefreshRate
	* measured refresh rate of the DRAM (should be 32ms or 64ms on successful measurement)
#### Addressing Function Reverse-Engineering
* AMDRE
	* Threshold between Row Hit and Row Conflict
	* Measured Number of Banks
	* Measured Block Size (number of following addresses before DRAM bank switches)
	* Linear DRAM bank addressing functions
* DRAMDig
* Drama
	* Histogram of access times of DRAM addresses
	* Number of Addresses in each set (sets = submitted number of DRAM banks)
	* Linear DRAM bank addressing functions
* Dare
	* Threshold between Row Hit and Row Conflict
	* Linear DRAM bank addressing functions
* TRRespass RE
	* Sets of addresses that produced row conflicts and are considered to be on the same bank
		* virtual address
		* physical address
		* access timing
	* DRAM Bank addressing candidate functions (without cleaning linear combinations)
	* DRAM Bank addressing functions (with cleaning linear combinations)
	* Row address mask (bits in the physical address used for row addressing)
#### Linear DRAM Addressing function Verification
* Drama-Verify
#### Rowhammer Tools
* Blacksmith
* HammerSuite (Tool from the TRRespass Paper)
* HammerTool

## Potential risks
* Based on the information we collect, there are potential risks if we loose these datasets:
	* **Fingerprinting** single systems:
    * **Probability**: medium
    * **Need for protection**: medium
		* Serial numbers of the components used in these systems: If used uniquely by the vendor, no other component has the same serial number
		* Detailed information about the components (without serial numbers) in combination: In the case that there is just one system with this specific set of components
		* Positions of Rowhammer Bit Flips could enable fingerprinting
	* Physical addresses where bit flips occur (sometimes including the addresses that were accessed to get the flip) can enable an attacker to **repeat Rowhammer with exactly these locations**
    * **Probability**: low
    * **Need for protection**: medium
		* Since `/proc/self/pagemap` returns `0x00` as physical address when not accessed by `root`, this requires root privileges
		* This could make privilege escalation attacks with partly known physical addresses (e.g., using transparent huge pages) a little bit easier (this is pretty limited)

* These are linked to the collected information as follows:
  * General execution information (e.g., which tools were executed, how long did they run, etc.)
    * None
  * System information that enable fingerprinting (e.g., serial numbers, etc.)
    * Fingerprinting
  * System information that do not enable fingerprinting (e.g., vendor, product name, etc.)
    * Potentially, fingerprinting by combining them
  * System information that were reverse-engineered (e.g., linear DRAM addressing functions, refresh rate, etc.)
    * Potentially, fingerprinting by combining them
  * Logs for debugging and classification of data sets (e.g., did the tool crash or just not find anything, etc.)
    * None
  * Logs of Bit Flip locations (e.g., physical addresses of victim rows and sometimes aggressor rows, etc.)
    * Potentially Fingerprinting (we want to verify this in the study)
    * Potentially repeat Rowhammer with exactly these locations

## What do we do to avoid loosing the datasets
* Datasets are stored in a crypto container (TODO) on a server (VM) on-premise. If somebody reboots the VM, there will be no access to the data. Only **we** have the passphrase.
* Backups (TODO) are completely encrypted. Only **we** have the passphrase
* Interaction with the data we stored is only possible through **us** using email (to avoid brute-forcing of hashes, etc.)
  * Write us an email with the hash of your dataset, serial numbers of components, etc. to prove that it is really your dataset
  * Ask us to send you the dataset or remove the dataset (in case of removal, we will also remove it from all backups)
  * You can still access your own dataset since it is stored on the thumb drive
* The entire dataset is removed **6 months** after the paper evaluating the data was accepted
* You do not have to participate in the study but can still use the tool to test your own systems (just deny to upload the results)

* TODO: Define a list of persons for **we** / **us**

## Reasoning why we need those information
* General System information: Correlation Analysis
  * How is the number of bit flips found by specific tools related to e.g., DRAM vendors, production years, DRAM types, Multi-Channel, CPU, etc.
	* How good works addressing function reverse-engineering of different tools on different hardware, e.g., number of DIMMs, CPU, cache sizes, etc.
* Information that enable Fingerprinting, Locations of Bit Flips
  * There are multiple papers claiming that Rowhammer can be used as physical uncloneable function (PUF) [TODO] or for Fingerprinting [TODO]
  * Typically, the same size is very small (e.g., it is possible to fingerprint 8 devices)
  * We want to verify these approaches on bigger scale: Is is also possible to fingerprint a couple of hundred devices?
