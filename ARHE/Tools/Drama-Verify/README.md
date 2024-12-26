# Drama Verify
Drama Verify is an approach to use the [Drama
Sidechannel](https://www.usenix.org/system/files/conference/usenixsecurity16/sec16_paper_pessl.pdf)
published by Pessl et al. to verify DRAM addressing functions previously
discovered by any addressing function reverse engineering tool.

In the first step, a big number of memory pages is allocated. Afterwards, the
addresses within these pages are grouped based on the addressing functions
submitted as command-line parameter. Next, pairs of addresses are selected in
a way that one pair contains two addresses from the same group and another pair
contains two addresses from different groups. Thereby, the first group is
iterated and the second group is randomly selected. Then, the addresses are
accessed alternatingly and the access time is measured. If both addresses are
from the same group and the access time is slower than the threshold (e.g. a
row hit occurred), this is interpreted as correct. The same applies for both
addresses from different groups if the access time is faster than the threshold.
Otherwise, the accesses are interpreted as incorrect.

In the end, the number of correct and incorrect accesses are taken to calculate
an overall success rate. In theory, a success rate of 100% should be returned
when all addressing functions are correct. If there are no linear combinations
(e.g. shared bits) between addressing functions, one incorrect addressing
function should reduce the success rate to 50%, two should reduce it to 25%,
etc.

Since there might be some noise on the system, the success rates might be a
little bit lower than described above. However, there should not be significant
differences.

If the addressing functions are not linear independent, e.g. different functions
share the same bits, the success rates might behave differently.
