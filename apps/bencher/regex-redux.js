/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// https://benchmarksgame-team.pages.debian.net/benchmarksgame/program/regexredux-node-2.html

// The Computer Language Benchmarks Game
// http://benchmarksgame.alioth.debian.org/
//
// regex-dna program contributed by Jesse Millikan
// Base on the Ruby version by jose fco. gonzalez
// fixed by Matthew Wilson
// ported to Node.js and sped up by Roman Pletnev
// converted from regex-dna program

var fs = require('fs'), i = fs.readFileSync('/dev/stdin', 'ascii'),
  ilen = i.length, clen, j,
  q = [/agggtaaa|tttaccct/ig, /[cgt]gggtaaa|tttaccc[acg]/ig,
    /a[act]ggtaaa|tttacc[agt]t/ig, /ag[act]gtaaa|tttac[agt]ct/ig,
    /agg[act]taaa|ttta[agt]cct/ig, /aggg[acg]aaa|ttt[cgt]ccct/ig,
    /agggt[cgt]aa|tt[acg]accct/ig, /agggta[cgt]a|t[acg]taccct/ig,
    /agggtaa[cgt]|[acg]ttaccct/ig],

  b = [
    "-",    /\|[^|][^|]*\|/g,
    "|",   /<[^>]*>/g,
    "<2>", /a[NSt]|BY/g,
    "<3>", /aND|caN|Ha[DS]|WaS/g,
    "<4>", /tHa[Nt]/g
    ];

i = i.replace(/^>.*\n|\n/mg, '');
clen = i.length;
for(j = 0; j<q.length; ++j) {
  var qj = q[j], m = i.match(qj);
  console.log(qj.source, m ? m.length : 0);
}
while(b.length) i = i.replace(b.pop(), b.pop());
console.log(["", ilen, clen, i.length].join("\n"));
