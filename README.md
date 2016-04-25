# A fast (not quite lock-free) multi-producer multi-consumer queue

## The only file needed is MPMCQueue.h

**CustomIndexType/** - some old code no longer maintained, but shows a strategy for dealing with index overflow if 64-bit CAS operations are not lock-free on a given platform

**Tests/** - some slightly unsightly code to compare this queue against a naive mutex implementation, and spits out a google chart to show results
