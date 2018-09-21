---
title: Faster CMake
author: Christopher Chedeau, Aaron Orenstein
---

When I joined the Skip team, my biggest annoyance was that CMake took forever to run (okay, 45s). I didn't expect it to be that slow since there was only a hundred source files. It turns out that we generate a target for every single test (we have a lot) and we duplicate all those test targets for each backend. At the end of the day, we had 5500 targets.

I commented out the line that generated targets for tests and CMake only took 1.5s! My instinct was to get rid of CMake altogether and with some healthy dose of hackery I was able to get Jest, our JavaScript testing framework, wired up for some of the tests. This triggered Aaron Orenstein to do something about it, he wasn't okay with the fact that a system built with C++ was slower than one in JavaScript ;)

## Cmake Performance

Aaron started profiling CMake and found out that a ton of time was spent looking up target names in `cmLocalGenerator`. When looking at the source, he found that it was a classic O(n²) issue where targets were stored in a vector instead of a hash map. He sent a [pull request](https://gitlab.kitware.com/cmake/cmake/merge_requests/1136) that adds an index backed by a unordered map which brought down the time from 45s to 15s!

After this was merged, he ran perf again and found another place where Cmake was doing a linear lookup. But this time, it wasn't a straightforward lookup. It was doing a fuzzy search for a filename where, based on the platform, it is a case sensitive or insensitive search and doing some work to allow different file extensions in certain cases.

The solution that Aaron implemented for this is to have an index where keys are lowercase filenames without their extensions that point to an array of values. Then, do a linear lookup on this smaller array. In practice, all the arrays are 1 elements so it keeps the same performance characteristics. That [second pull request](https://gitlab.kitware.com/cmake/cmake/merge_requests/1421) brought down the time again from 15s to 7s!

## Conclusion

The first improvement was shipped as part of CMake 3.10.0 this morning! If you are using a Mac you can run `brew upgrade cmake` to get the improvements. The second one is expected to ship as part of 3.11.0.

This is really exciting that as part of building Skip, we've been able to improve a tool used by the entire industry! #impact
