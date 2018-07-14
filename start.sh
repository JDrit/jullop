#!/usr/bin/sh

HEAPPROFILE=/tmp/profile/heap CPUPROFILE=/tmp/profile/cpu.out PERFTOOLS_VERBOSE=100 MALLOCSTATS=100 \
	   ./jullop
