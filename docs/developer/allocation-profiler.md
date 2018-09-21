# Heap Allocation Profiler

## What it is

A simple heap profiler that logs every call to `ObStack::alloc`.  After the program terminates, a report (as a CSV) is printed showing, by *call site*, the total number of calls to that call site and the total number of bytes allocated by that call site. Note that this currently just aggregates allocations; it makes no account of liveness of data or GC whatsoever.

## How it works

To use it, set the environment variable `SKIP_HEAP_PROFILE` to a suitable value -- `1` will enable the heap profiler for virtually every Skip program, `2` will enable the profiler for the Skip compiler itself.

If heap profiler logging is enabled, a diagnostic is printed to stderr at startup indicating that logging is enabled, and a diagnostic is printed at exit giving the path name to the tmp file with the CSV of the aggregated allocation log data.

For example, to run `photo_sizes` with heap profiling:

```
$ cd apps/photo_sizes
$ export SKIP_HEAP_PROFILE=1
$ ./run.sh
$ ./run.sh
heap profiler: enabled!
18.288917999999999
15.840624999999999
14.855427000000001
17.963353999999999
19.692511
20.578268999999999
20.004055000000001
21.852540999999999
20.032986000000001
21.454595000000001
18.458265999999998
21.815688000000002
20.935580000000002
18.286553000000001
20.065301999999999
19.822814000000001
19.400055999999999
19.979600000000001
20.499217999999999
19.964642999999999
Dumping log of size 15
heap profiler: wrote allocation log to /tmp/skip-alloc-log-KOxUmt.csv
$
```
then use your favorite CSV viewer (which is [Tad](https://www.tadviewer.com/)) to view the CSV file.

With a little massaging of the settings, this will give you a view something like this:

![image](https://user-images.githubusercontent.com/4683413/37880653-cda7405e-3040-11e8-888a-030d9f58c658.png)

## Caveats

This is still rough work-in-progress.  Some caveats:

 - This profile identifies an *allocation site* by the names of the top 4 functions on the stack plus the size of the allocation in bytes. (This is done after trimming off the call to `ObStack::alloc` and the heap profile logger itself).  Note that this means that if the same function does two allocations of the same size, those will be identified as the same allocation site by the profiler.

 - The resulting CSV file places the names of the functions of the allocation site in columns 'pc0' through 'pc3', padding out latter columns with '-' for call sites less than depth 4. Note that this means that, when doing aggregations of multiple call sites (such as in a pivot table), one should start with 'pc0' (the inner most / most recent) function call.

  - There is, unsurprisingly, severe overhead to logging -- 10x is not uncommon.  One difficulty is that `libunwind` forces us to obtain symbolic names for addresses at the time of call; it would be more efficient to just work with machine addresses to identify allocation sites and only map to symbolic names when emitting the log before exiting.
