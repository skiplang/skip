/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

'use strict';

const fs = require('fs');
const path = require('path');

const argv = process.argv;
// argv[0] = node
// argv[1] = generate-parse-trees.js
const treesPath = argv[2];
const json = JSON.parse(fs.readFileSync(treesPath, 'utf8'));
const trees = json.trees;
const mod = json.module == null ? "ParseTree" : json.module;
trees.sort((a, b) => { return a[0].localeCompare(b[0]); });
// console.log(JSON.stringify(trees));

const header =
`// DO NOT EDIT.
// This file is generated from trees.json.
// After editing trees.json you must 'ninja -C build update_parse_tree'
// to update this file.

module ${mod};

extension base class ParseTree {
`;

const getChildren = `
  fun getChildren(): mutable Iterator<ParseTree.ParseTree> {
`;

const endClass = `}`;
const endMethod = `  }
`;

const getNamedFields = `
  fun getNamedFields(): List<(String, ParseTree.ParseTree)> {
`;

const getKind = `
  fun getKind(): String {
`;

const transform = `
  fun transform(
    codemod: mutable CodeMod,
  ): (ParseTree.ParseTree, Vector<Subst>) {
`;

////////////////////////////////////////////////////////////////////////////////
// main

////////////////////////////////////////////////////////////////////////////////
// extension class ParseTree
console.log(header);
console.log(endClass);

////////////////////////////////////////////////////////////////////////////////
// children
// class XTree{fields} extends ${mod}.ParseTree {
// getNamedFields
// getKind
// getChildren
// transform
for (let tree of trees) {
  const name = tree[0];
  const children = tree[1];
  // class XTree{fields} extends ${mod}.ParseTree {
  console.log(`class ${name}Tree{`);
    for (let child of children) {
      console.log(`  ${child}: ParseTree.ParseTree,`);
    }
  console.log(`} extends ${mod}.ParseTree {`);

  // getNamedFields
  console.log(getNamedFields);
  console.log(`    List<(String, ParseTree.ParseTree)>[`);
  for (let child of children) {
    console.log(`      ("${child}", this.${child}),`)
  }
  console.log(`    ];`);
  console.log(endMethod);

  // getKind
  console.log(getKind);
  console.log(`    "${name}";`);
  console.log(endMethod);

  // getChildren
  console.log(getChildren);
  for (let child of children) {
    console.log(`    yield this.${child};`)
  }
  console.log(`    yield break;`)
  console.log(endMethod);

  // transform
  console.log(transform);
  if (children.length === 0) {
    console.log("_ = codemod;");
  } else {
    for (let child of children) {
      console.log(`    tx_${child} = codemod.transform(this.${child});`);
    }
  }
  console.log(`    (`);
  console.log(`      ${name}Tree{`);
  console.log(`        range => this.range,`);
  for (let child of children) {
    console.log(`        ${child} => tx_${child}.i0,`);
  }
  console.log(`      },`);
  console.log(`      Vector[`);
  for (let child of children) {
    console.log(`        tx_${child}.i1,`);
  }
  console.log(`      ].flatten(),`);
  console.log(`    );`);
  console.log(endMethod);

  console.log(endClass);
}
