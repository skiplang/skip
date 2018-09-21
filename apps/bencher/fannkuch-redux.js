/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// JavaScript output of
// https://benchmarksgame-team.pages.debian.net/benchmarksgame/program/fannkuchredux-typescript-2.html

/* The Computer Language Benchmarks Game
   http://benchmarksgame.alioth.debian.org/
   contributed by Isaac Gouy
*/
/// <reference path="./Include/node/index.d.ts" />
function fannkuch(n) {
    var perm = new Int32Array(n), count = new Int32Array(n);
    var perm1 = new Int32Array(n);
    for (var i_1 = 0; i_1 < n; i_1++) {
        perm1[i_1] = i_1;
    }
    var f = 0, i = 0, k = 0, r = 0, flips = 0, nperm = 0, checksum = 0;
    r = n;
    while (r > 0) {
        i = 0;
        while (r != 1) {
            count[r - 1] = r;
            r -= 1;
        }
        while (i < n) {
            perm[i] = perm1[i];
            i += 1;
        }
        // Count flips and update max  and checksum
        f = 0;
        k = perm[0];
        while (k != 0) {
            i = 0;
            while (2 * i < k) {
                var t = perm[i];
                perm[i] = perm[k - i];
                perm[k - i] = t;
                i += 1;
            }
            k = perm[0];
            f += 1;
        }
        if (f > flips) {
            flips = f;
        }
        if ((nperm & 0x1) == 0) {
            checksum += f;
        }
        else {
            checksum -= f;
        }
        // Use incremental change to generate another permutation
        var go = true;
        while (go) {
            if (r == n) {
                console.log(checksum);
                return flips;
            }
            var p0 = perm1[0];
            i = 0;
            while (i < r) {
                var j = i + 1;
                perm1[i] = perm1[j];
                i = j;
            }
            perm1[r] = p0;
            count[r] -= 1;
            if (count[r] > 0) {
                go = false;
            }
            else {
                r += 1;
            }
        }
        nperm += 1;
    }
    return flips;
}
var n = +process.argv[2];
console.log("Pfannkuchen(" + n + ") = " + fannkuch(n));
