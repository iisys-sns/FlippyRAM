# measureRefreshRate
The tool `measureRefreshRate` can be used to verify if a system has enabled the
double refresh rate mitigation. Typically, the refresh rate of DRAM is specified
to be 64ms, so one cell has to hold the charge at least 64ms. After that, it
is guaranteed that the row is refreshed.

An early rowhammer mitigation was to double the refresh rate to 32ms, so it was
sufficient when the cell was able to hold the charge for 32ms even when a
rowhammer attack was executed. Since that mitigation was not really effective,
it was replaced by newer mitigation techniques (e.g. TRR, pTRR) later.

## Description
`measureRefreshRate` allocates one page, accesses the first address of that
page in a loop, and measures the access time. Between the accesses, `clflush`
is used to remove the data from the CPU cache.

There should be peaks in the access times whenever a refresh is performed since
the DRAM bank cannot be accessed during a refresh (e.g. the accesses are slower
while a row in the same DRAM bank is refreshed. Since the refresh is done in
`8192` batches, there should be a peak every `64ms / 8192 = 7.8us` in case of
a refresh rate of 64ms. There should be a peak every `32ms / 8192 = 3.9us` in
case of a refresh rate of 32ms.

The tool tries to detect those peaks automatically and guess the refresh rate
based on the detected peaks. It is also possible to add the `-v` command line
parameter to see the plot. Note that it might be required to adjust the value
of the scaling (command line parameter `-s`) in order to get meaningful graphs.

## Example
The following output is shown on a system with a refresh rate of 64ms and DDR4
SDRAM (used by default):
```
./measureRefreshRate
Measured a timing of 7.80us. Estimated refresh interval per row: 63.89ms
```

The following output is shown on a system with a refresh rate of 32ms and DDR3
SDRAM:
```
./measureRefreshRate --memory-type=ddr3
Measured a timing of 3.87us. Estimated refresh interval per row: 31.70ms
```
