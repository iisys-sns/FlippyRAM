## Add a new Rowhammer Tool
In general, you should run you tool manually and verify that it works before
automating it. After a first test run on a system was successful, use the
following steps to add you tool to the ISO image.

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

### Testing
After you implemented the new experiment according to the steps described above,
you should perform some tests. The tests
should be performed in multiple stages:
* Deploy your current state to the system you want to test at
	* run `main.sh` first to see the command-line output of the scripts, tools, etc.
	* run `run.sh` after the tests with `main.sh` worked to test the UI
	* run `sh docker-run.sh` to test the docker container
* Build the ISO image by running `make`, flash it to a thumb drive and boot it
  on some systems