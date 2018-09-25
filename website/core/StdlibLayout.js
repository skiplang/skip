/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

var fs = require('fs');

module.exports = function({React, MarkdownBlock}) {
  var metadataPath = __dirname + '/../stdlib-metadata.json';
  if (!fs.existsSync(metadataPath)) {
    throw new Error('Standard library docs have not been extracted, please run ./website/extract-docs.sh');
  }
  var stdlib = JSON.parse(fs.readFileSync(metadataPath, 'utf8'));
  var h = React.createElement;

  function renderClass(class_) {
    var section = null;
    return [
      h('span', {}, [
        h('span', {className: 'stdlib-class-kind'}, class_.kind + ' '),
        class_.name,
        h('span', {className: 'stdlib-class-tparams'}, renderTparams(class_.tparams)),
      ]),
      h('div', {}, [
        class_.comments && h(MarkdownBlock, {}, class_.comments),
        class_.methods
          .map(method => {
            var title;
            if (method.definedLocally) {
              if (method.isStatic && section !== 'static') {
                section = 'static';
                title = h('h2', {}, 'Static Methods');
              } else if (!method.isStatic && section !== 'methods') {
                section = 'methods';
                title = h('h2', {}, 'Methods');
              }
            } else if (section !== 'inherited') {
              section = 'inherited';
              title = h('h2', {}, 'Inherited Methods');
            }
            return [title, renderMethod(method)];
          }),
      ]),
    ];
  }

  function join(list, separator) {
    var res = [];
    list.forEach((elem, i) => {
      res.push(elem);
      if (i !== list.length - 1) {
        res.push(separator);
      }
    });
    return res;
  }

  function renderTparams(tparams) {
    if (tparams.length === 0) {
      return h('span', {}, '');
    }
    return h('span', {}, [
      '<',
      join(tparams.map(t => renderType(t)), ', '),
      '>',
    ])
  }

  function renderType(ty) {
    if (typeof ty === 'string') {
      return h('span', {}, ty);
    }
    if (ty.type === 'param') {
      return ty.name;
    }
    if (ty.type === 'class') {
      return h('a', {href: ty.name.toLowerCase() + '.html'}, ty.name);
    }
    if (ty.type === 'moduleClass') {
      return h('a', {href: ty.className.toLowerCase() + '.html'}, ty.moduleName + '.' + ty.className);
    }
    if (ty.type === 'apply') {
      return h('span', {}, [
        renderType(ty.typeName),
        ty.types.length > 0
          ? h('span', {}, [
              '<',
              join(ty.types.map(t => renderType(t)), ', '),
              '>'
            ])
          : undefined
      ]);
    }
    if (ty.type === 'tuple') {
      return h('span', {}, [
        '(',
        join(ty.types.map(t => renderType(t)), ', '),
        ')'
      ]);
    }
    if (ty.type === 'mutable' || ty.type === 'readonly') {
      return h('span', {}, [
        ty.type,
        ' ',
        renderType(ty.ty)
      ]);
    }
    if (ty.type === 'fun') {
      return h('span', {}, [
        renderType(ty.params),
        ty.isMutable ? ' -> ' : ' ~> ',
        renderType(ty.returnType),
      ]);
    }
    if (ty.type === 'positional') {
      return h('span', {}, [
        '(',
        join(
          ty.parameters.map(param =>
            [
              param.name.startsWith('@') ? '' : [param.name, ': '],
              renderType(param.ty)
            ]
          ),
          ', '
        ),
        ')'
      ]);
    }
    if (ty.type === 'named') {
      return h('span', {}, [
        '{',
        join(ty.parameters.map(param => [param.name, ' => ', renderType(param.ty)]), ', '),
        '}'
      ]);
    }
    if (ty.type === 'tparameter') {
      return h('span', {}, [
        ty.variance,
        ty.name,
        ty.types.length > 0
          ? [': ', join(ty.types.map(t => renderType(t)), ' & ')]
          : ''
      ]);
    }
    if (ty.type === '^') {
      return h('span', {}, [
        '^',
        renderType(ty.ty),
      ]);
    }

    throw new Error('Unknown type ' + JSON.stringify(ty));
  }

  function renderMethod(method) {
    var comments = method.comments ? method.comments.split('\n') : [];
    var header = null;
    if (comments.length && comments[0].indexOf('#') === 0) {
      header = comments.shift().substring(1);
    }
    var mutability = null;
    if (method.isFrozen) {
      mutability = 'frozen ';
    } else if (method.isReadonly) {
      mutability = 'readonly ';
    } else if (method.isMutable) {
      mutability = 'mutable ';
    }
    var untracked = method.isUntracked ? 'untracked ' : null;
    return h('div', {className: 'stdlib-method'}, [
      header ? h('h3', {}, header) : undefined,
      h('a', {className: 'anchor', name: method.name}),
      method.isStatic ? 'static ' : undefined,
      untracked,
      mutability,
      'fun ',
      h(
        'a',
        {
          href: 'https://github.com/skiplang/skip/blob/master/src/runtime/' +
            method.sourceFile + '#L' + method.sourceLine,
          target: '_blank',
          style: {float: 'right'},
        },
        'source'
      ),
      h(
        'a',
        {
          className: 'stdlib-method-name',
          href: '#' + method.name,
        },
        method.name
      ),
      renderTparams(method.tparams),
      renderType(method.params),
      ': ',
      renderType(method.returnType),
      h('div', {className: 'stdlib-method-contents'}, [
        comments.length ? h(MarkdownBlock, {}, comments.join('\n')) : undefined,
      ]),
    ]);
  }

  // This takes a string of the form
  //
  //   {
  //     // this is a comment
  //     someCode();
  //   }
  //
  // and outputs a string of the form
  //
  //   this is a comment
  //   ```
  //   someCode();
  //   ```
  //
  // The first pass is to remove {} and leading indentation
  // The second pass is to remove // from comments and add ``` around code
  function formatExample(example) {
    var lines = example.split('\n').slice(1, -1);
    var indent = lines[0].match(/^[\t ]*/)[0];

    var result = '';
    var lastResult = '';
    var lastIsComment = null;

    function flush() {
      if (lastResult == '') {
        return;
      }
      if (lastIsComment) {
        result += lastResult.trim() + '\n';
      } else {
        result += '```\n' + lastResult.trim() + '\n```\n';
      }
    }

    var aligned = lines.forEach(line => {
      var lineAligned = line.slice(indent.length);
      var isComment = lineAligned.startsWith('//');
      if (lastIsComment === null) {
        lastIsComment = isComment;
      }

      if (isComment !== lastIsComment) {
        flush();
        lastResult = '';
        lastIsComment = isComment;
      }

      if (isComment) {
        lastResult += lineAligned.replace(/^\/\/ ?/, '') + '\n';
      } else {
        lastResult += lineAligned + '\n';
      }
    });
    flush();

    return result;
  }

  return class extends React.Component {
    render() {
      var metadata = this.props.metadata;

      var title, content;
      if (stdlib.classes[metadata.title]) {
        var class_ = stdlib.classes[metadata.title];
        [title, content] = renderClass(class_);
      } else {
        throw new Error('Cannot find ' + metadata.title + ' in website/stdlib-metadata.json');
      }

      return h('div', {className: 'post'}, [
        h('header', {className: 'postHeader'}, [
          h('h1', {}, [
            title
          ])
        ]),
        h('article', {}, [
          content,
        ])
      ])
    }
  }
};
