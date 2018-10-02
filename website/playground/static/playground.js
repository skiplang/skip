/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

(function() {
var h = React.createElement;

window.define = function(lib, deps, fn) {
  createSkip = fn;
};

window.setupEditor = function(elements, options = {}, basePath = '', hiddenCode = '') {
  var errors = [];
  var _output = '';

  function clear_output() {
    clearTreeOutput();
    errors = [];
    codeEditor.performLint();
    _output = '';
    elements.output.innerHTML = '';
    showOutput();
  }
  function add_output(value, type) {
    if (elements.output.style.display !== 'block') {
      _output = '';
    }
    showOutput();
    if (value.__value !== undefined) {
      value = value.__value;
    }

    _output += value;

    var lastChild = elements.output.lastChild;
    if (!lastChild) {
      lastChild = document.createElement('div');
      lastChild.className = type;
      lastChild.innerText = '';
      elements.output.appendChild(lastChild);
    }

    value.split('\n').forEach(function(line, i) {
      if (i !== 0) {
        lastChild = document.createElement('div');
        lastChild.className = type;
        lastChild.innerText = '';
        elements.output.appendChild(lastChild);
      }
      lastChild.className = type;
      lastChild.innerText += line;
    });
  }
  function print_debug(value) {
    console.log(value);
    add_output(value, 'log');
  }
  function print_log(value) {
    add_output(value, 'log');
  }
  function print_error(value) {
    add_output(value, 'error');
  }

  function showOutput() {
    elements.output.style.display = 'block';
    if (elements.codeOutput) {
      elements.codeOutput.nextSibling.style.display = 'none';
    }
    if (elements.treeOutput) {
      elements.treeOutput.style.display = 'none';
    }
  }
  function showCodeOutput() {
    elements.output.style.display = 'none';
    if (elements.codeOutput) {
      elements.codeOutput.nextSibling.style.display = 'block';
    }
    if (elements.treeOutput) {
      elements.treeOutput.style.display = 'none';
    }
  }
  function showTreeOutput() {
    elements.output.style.display = 'none';
    if (elements.codeOutput) {
      elements.codeOutput.nextSibling.style.display = 'none';
    }
    if (elements.treeOutput) {
      elements.treeOutput.style.display = 'block';
    }
  }
  function setCodeOutput(text) {
    showCodeOutput();
    _output = text.trim();
    codeOutputEditor.setValue(text.trim());
  }

  var clearTreeOutput = () => {};
  function setTreeOutput(text) {
    try {
      var tree = JSON.parse(text);
    } catch(e) {
      showOutput();
      print_error(text);
      return;
    }

    clearTreeOutput();
    showTreeOutput();

    function getRange(node) {
      var range;
      if (node.type === "object" && node.value.range) {
        range = node.value.range.value
      } else if (
        node.type === "call" &&
        node.value.length > 0 &&
        node.value[0].type === "literal" &&
        node.value[0].value.match(/\([0-9]+, [0-9]+\)\-\([0-9]+, [0-9]+\)/)
      ) {
        range = node.value[0].value.replace(/^[^\(]*\(/, '(');
      }

      if (range) {
        range = range.split(/[^0-9]+/).slice(1, -1).map(x => +x);
        if (range[0] === range[2] && range[1] === range[3]) {
          range = null;
        }
      }
      return range;
    }
    function isTogglable(node) {
      if (node.type === "vector" || node.type === "call") {
        return node.value.length > 0;
      }
      if (node.type === "object") {
        return Object.keys(node.value).length > 0;
      }
      return false;
    }

    var filterOutEmpty = true;
    var filterOutRange = true;
    function filterOut(name, node) {
      if (filterOutRange && name === "range") {
        return false;
      }
      if (filterOutEmpty) {
        if (node.type === "vector" && node.value.length === 0) {
          return false;
        }
        if (node.type === "call" && node.name === "None") {
          return false;
        }
        if (node.type === "object" && node.name === "ParseTree.EmptyTree") {
          return false;
        }
        if (node.type === "object") {
          for (var name in node.value) {
            if (filterOut(name, node.value[name])) {
              return true;
            }
          }
          return false;
        }
      }
      return true;
    }
    function filterOutValue(value) {
      var res = {};
      Object.keys(value).forEach(name => {
        if (filterOut(name, value[name])) {
          res[name] = value[name];
        }
      });
      return res;
    }

    var selectedNode;
    var marks = [];
    var scrollTo = false;
    class Node extends React.Component {
      constructor(props) {
        super(props);
      }
      toggle = () => {
        this.props.node.__isToggled = !this.props.node.__isToggled;
        this.forceUpdate();
      }
      scrollTo() {
        if (scrollTo && selectedNode === this.props.node) {
          ReactDOM.findDOMNode(this).scrollIntoView();
          scrollTo = false;
        }
      }
      componentDidMount() {
        this.scrollTo();
      }
      componentDidUpdate() {
        this.scrollTo();
      }
      render() {
        var node = this.props.node;
        var togglable = isTogglable(node);
        var toggle = togglable ? this.toggle : null;
        var isToggled = togglable && node.__isToggled;
        var treeOpen = togglable ? 'tree-open' : '';

        var content;
        if (node.type === "object") {
          content = [
            h('span', {onClick: toggle, className: 'tree-no-underline ' + treeOpen}, '{'),
            isToggled
              ? h('div', {className: 'tree-children'},
                  Object.keys(filterOutValue(node.value)).map(name =>
                    h(Node, {node: node.value[name], name, depth: this.props.depth + 1})
                  )
                )
              : h('span', {onClick: toggle, className: treeOpen},
                  Object.keys(filterOutValue(node.value)).join(', ')
                ),
            h('span', {onClick: toggle, className: 'tree-no-underline ' + treeOpen}, [
              (isToggled
                ? h('span', {}, [
                    h('span', {className: 'tree-no-select'}, '\u00A0\u00A0'),
                    h('span', {className: 'tree-hidden'}, '\u00A0\u00A0'.repeat(Math.max(0, this.props.depth)))
                  ])
                : ''),
              '}'
            ]),
          ]
        } else if (node.type === "vector" || node.type === "call") {
          content = [
            h('span', {onClick: toggle, className: 'tree-no-underline ' + treeOpen},
              node.type === "vector" ? '[' : '('
            ),
            isToggled
              ? h('div', {className: 'tree-children'},
                  node.value.map(value =>
                    h(Node, {node: value, depth: this.props.depth + 1})
                  )
                )
              : node.value.length > 0
                ? h('span', {onClick: toggle, className: treeOpen},
                    node.value.length + ' element' + (node.value.length > 1 ? 's' : '')
                  )
                : null,
            h('span', {onClick: toggle, className: 'tree-no-underline ' + treeOpen}, [
              (isToggled
                ? h('span', {}, [
                    h('span', {className: 'tree-no-select'}, '\u00A0\u00A0'),
                    h('span', {className: 'tree-hidden'}, '\u00A0\u00A0'.repeat(Math.max(0, this.props.depth)))
                  ])
                : ''),
              (node.type === "vector" ? ']' : ')')
            ]),
          ]
        } else if (node.type === "literal") {
          var value = node.value;
          if (value.match(/\([0-9]+, [0-9]+\)\-\([0-9]+, [0-9]+\)/)) {
            value = value.replace(/^[^\(]*\(/, '(');
          }
          content = [
            h('span', {className: 'tree-literal'}, value)
          ]
        } else if (node.type === "string") {
          content = [
            h('span', {className: 'tree-string'}, JSON.stringify(node.value))
          ]
        } else {
          content = [
            h('span', {}, node.type)
          ]
        }

        var range = getRange(node);
        var onMouseOverCapture = range
          ? () => {
              marks.forEach(mark => mark.clear());
              marks.push(codeEditor.markText(
                {line: range[0] - 1, ch: range[1] - 1},
                {line: range[2] - 1, ch: range[3] - 1},
                {className: "tree-highlighted"}
              ));
            }
          : null;
        var onMouseOutCapture = range
          ? () => {
              marks.shift().clear();
            }
          : null;

        return h('div', {onMouseOverCapture, onMouseOutCapture, className: selectedNode === node ? 'tree-highlighted' : ''}, [
          h('span', {className: 'tree-hidden'}, '\u00A0\u00A0'.repeat(this.props.depth)),
          togglable
            ? h('span', {onClick: toggle, className: 'tree-no-select tree-no-underline ' + treeOpen}, isToggled ? '- ' : '+ ')
            : h('span', {className: 'tree-no-select'}, '\u00A0\u00A0'),
          this.props.name
            ? [
                h('span', {onClick: toggle, className: treeOpen}, this.props.name),
                h('span', {onClick: toggle, className: 'tree-no-underline ' + treeOpen}, ' => '),
              ]
            : null,
          h('span', {onClick: toggle, className: 'tree-name ' + treeOpen}, node.name),
          content,
          h('span', {className: 'tree-hidden'}, ','),
        ]);
      }
    }

    var onMouseLeave = () => {
      marks.forEach(mark => mark.clear());
      marks = [];
    };
    var render = () => {
      ReactDOM.render(
        h('div', {onMouseLeave}, [
          h('div', {className: 'tree-label'}, [
            h('label', {}, [
              h('input', {type: 'checkbox', checked: filterOutRange, onClick: () => { filterOutRange = !filterOutRange; render(); }}),
              h('span', {}, 'Hide ranges'),
            ]),
            h('label', {}, [
              h('input', {type: 'checkbox', checked: filterOutEmpty, onClick: () => { filterOutEmpty = !filterOutEmpty; render(); }}),
              h('span', {}, 'Hide empty nodes'),
            ]),
          ]),
          h(Node, {node: tree, depth: 0})
        ]),
        elements.treeOutput
      );
    };

    var onCursorChange = () => {
      var cursor = codeEditor.getCursor();
      var line = cursor.line + 1;
      var col = cursor.ch + 1;

      function traverse(node) {
        var range = getRange(node);
        if (range) {
          if (line < range[0] || line > range[2]) {
            return false;
          }
          if (line === range[0] && col < range[1]) {
            return false;
          }
          if (line === range[2] && col > range[3]) {
            return false;
          }
        }
        var isChildrenToggled = false;
        if (node.type === "object") {
          Object.keys(node.value).forEach(name => {
            isChildrenToggled = isChildrenToggled || traverse(node.value[name]);
          });
        } else if (node.type === "vector" || node.type === "call") {
          node.value.forEach(value => {
            isChildrenToggled = isChildrenToggled || traverse(value);
          });
        }
        if ((isChildrenToggled || range) && isTogglable(node)) {
          if (range && !isChildrenToggled) {
            selectedNode = node;
          }
          node.__isToggled = true;
        }
        if (range) {
          return true;
        } else {
          return isChildrenToggled;
        }
      }
      traverse(tree);
      scrollTo = true;
      render();
    };

    var cursorActivity = () => {
      onCursorChange();
    };
    codeEditor.on('change', clear_output);
    codeEditor.on('cursorActivity', cursorActivity);

    clearTreeOutput = () => {
      onMouseLeave();
      onCursorChange = () => {};
      codeEditor.off('cursorActivity', cursorActivity);
      codeEditor.off('change', clear_output);
      ReactDOM.unmountComponentAtNode(elements.treeOutput);
      clearTreeOutput = () => {};
    };
    render();
  }

  function post(endpoint, body, cb) {
    fetch(endpoint, {method: 'POST', body})
      .then(function(res) {
        res.text().then(function(value) {
          if (res.status !== 201) {
            cb(value, null);
          } else {
            cb(null, value);
          }
        });
      })
      .catch(function(e) {
        cb(null, e.message);
      });
  }

  function handleError(error) {
    var regex = /File "playground.sk", line ([0-9]+), characters ([0-9]+)-([0-9]+):\n([\s\S]*?)($|\n[ ]*[0-9]* \|)/g;
    errors = [];
    error.replace(regex, (_, line, columnStart, columnEnd, message) => {
      errors.push({
        from: CodeMirror.Pos(+line - 1, +columnStart - 1),
        to: CodeMirror.Pos(+line - 1, +columnEnd),
        message: message,
        severity: errors.length === 0 ? 'error' : 'warning'
      });
    });
    codeEditor.performLint();
    print_error(error);
  }

  function wire(button, endpoint, cb) {
    if (!button) {
      return;
    }
    button.onclick = function() {
      button.classList.add('playground-loading');
      clear_output();
      post(endpoint, codeEditor.getValue() + "\n\n\n\n" + hiddenCode, function(success, error) {
        button.classList.remove('playground-loading');
        if (success) {
          cb(success);
        } else {
          handleError(error);
        }
      });
    }
  }

  wire(elements.run, basePath + 'skip_to_js', function(value) {
    const skip = createSkip();
    const require = () => skip;
    // Keep the following in one line to avoid messing up source map line numbers
    const sk = eval(`(function(require, log, error) { var module = {}; ${value}
  sk.__.debug = 1;
  sk$print_raw = sk.print_raw = log;
  sk$print_error = sk.print_error = error;
  return sk;
})`)(require, print_log, print_error);
    if (!sk.main) {
      sk.print_error("Missing a main function: fun main(): void { print_string(\"Hello Skip!\") }");
      return;
    }
    try {
      skip.createReactiveContext().run(sk.main, []);
    } catch(e) {
      var stack = e.__stack ?
        '\n\n' + e.__stack.replace(/\([^\n]+/g, '') +
          '\n\nPro-tip: Open Chrome devtools in the Sources pane and enable ' +
          '"Pause on Exceptions" as well as "Pause on caught exceptions" to ' +
          'start debugging.'
        : '';
      if (e.getMessage) {
        sk.print_error(e.getMessage().__value + stack);
      } else {
        sk.print_error(new sk.String(e.toString() + stack));
      }
    }
  });

  wire(elements.js, basePath + 'skip_to_js?filter', function(value) {
    setCodeOutput(value);
  })

  wire(elements.ast, basePath + 'skip_to_ast', function(value) {
    setTreeOutput(value);
  })

  wire(elements.parsetree, basePath + 'skip_to_parsetree', function(value) {
    setTreeOutput(value);
  })

  wire(elements.printer, basePath + 'skip_printer', function(value) {
    codeEditor.setValue(value);
  })

  wire(elements.search, basePath + 'skip_search', function(value) {
    setCodeOutput(value);
  })


  function setNativeOutput(value) {
    var out = JSON.parse(value);
    var isAssembly = true;
    var hideLocations = true;
    var hideDeadLabels = true;

    var render = () => {
      var value = '';
      out.disassembly.forEach(function(fun) {
        if (isAssembly) {
          var asm = fun.asm;
          var cleanAsm = asm.split('\n').filter(line => {
            if (hideLocations) {
              // Remove all the .loc and .file lines
              if (line.startsWith('\t.loc') || line.startsWith('\t.file')) {
                return false;
              }
            }
            if (hideDeadLabels) {
              // Remove all the labels that are not referenced anywhere else
              var match = line.match(/^([^_][^: \t]+):/);
              if (match) {
                return asm.split(match[1]).length > 2;
              }
            }
            return true;
          }).join('\n');

          value += cleanAsm + '\n\n';
        } else {
          value += fun.llvm + '\n';
        }
      });

      showTreeOutput();
      ReactDOM.unmountComponentAtNode(elements.treeOutput);

      _output = value;
      ReactDOM.render(
        h('div', {}, [
          h('div', {className: 'tree-label'}, [
            h('label', {}, [
              h('input', {type: 'radio', checked: !isAssembly, onClick: () => { isAssembly = false; render(); }}),
              h('span', {}, '.ll'),
            ]),
            h('label', {}, [
              h('input', {type: 'radio', checked: isAssembly, onClick: () => { isAssembly = true; render(); }}),
              h('span', {}, 'Assembly'),
            ]),
            isAssembly
              ? [
                h('label', {}, [
                  h('input', {type: 'checkbox', checked: hideLocations, onClick: () => { hideLocations = !hideLocations; render(); }}),
                  h('span', {}, 'Hide locations'),
                ]),
                h('label', {}, [
                  h('input', {type: 'checkbox', checked: hideDeadLabels, onClick: () => { hideDeadLabels = !hideDeadLabels; render(); }}),
                  h('span', {}, 'Hide dead labels'),
                ])
              ]
              : []
          ]),
          h('textarea', {}, value),
        ]),
        elements.treeOutput
      );

      CodeMirror.fromTextArea(elements.treeOutput.querySelector('textarea'), Object.assign({}, options, {
        smartIndent: false,
        readOnly: true,
        theme: "neat",
        tabSize: 2,
        viewportMargin: Infinity,
        lineNumbers: false,
      }))
    };

    clearTreeOutput = () => {
      ReactDOM.unmountComponentAtNode(elements.treeOutput);
      render = () => {};
      clearTreeOutput = () => {};
    };

    render();
  }

  wire(elements.native, basePath + 'skip_to_native', function(value) {
    setNativeOutput(value);
  });

  if (elements.codeOutput) {
    var codeOutputEditor = CodeMirror.fromTextArea(elements.codeOutput, Object.assign({
      smartIndent: false,
      readOnly: true,
      theme: "neat",
      tabSize: 2,
      viewportMargin: Infinity,
    }, options));
  }

  var codeEditor = CodeMirror.fromTextArea(elements.textarea, Object.assign({
    smartIndent: false,
    keyMap: "sublime",
    autoCloseBrackets: true,
    matchBrackets: true,
    showCursorWhenSelecting: true,
    theme: "neat",
    tabSize: 2,
    viewportMargin: Infinity,
  }, options));
  codeEditor.setOption('lint', {
    lintOnChange: false,
    getAnnotations: function() {
      return errors;
    }
  });
  codeEditor.addKeyMap({
    'Ctrl-Enter': function() { elements.run.click() },
    'Cmd-Enter': function() { elements.run.click() },
    'Shift-Cmd-C': function() { elements.printer.click() },
  });
  showOutput();

  return codeEditor;
}

})();
