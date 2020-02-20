/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

const siteConfig = {
  title: "skip",
  tagline: "A programming language to skip the things you have already computed",
  url: "https://SkipLang.github.io",
  baseUrl: "/",
  editUrl: "https://github.com/skiplang/skip/blob/master/docs/overview/",
  customDocsPath: 'docs/overview/',
  projectName: "skip",
  headerLinks: [
    { doc: "tutorial", label: "Tutorial" },
    { doc: "hello_world", label: "Docs" },
    { doc: "string", label: "Stdlib" },
    { href: "/playground/", label: "Playground" },
    { blog: true, label: "Blog" },
    { href: "https://github.com/skiplang/skip", label: "GitHub" },
  ],
  customDocsPath: "/docs/overview",
  blogSidebarCount: 20,
  algolia: {
    apiKey: '7d79ea7bb5fdb8d123b2ed2c40d781a5',
    indexName: 'skiplang',
  },
  stylesheets: [
    '/playground/third-party/codemirror.css',
    '/playground/third-party/neat.css',
    '/playground/third-party/lint.css',

    '/playground/playground.css',
  ],
  scripts: [
    '/playground/third-party/react.production.min.js',
    '/playground/third-party/react-dom.production.min.js',
    '/playground/third-party/codemirror.js',
    '/playground/third-party/rulers.js',
    '/playground/third-party/searchcursor.js',
    '/playground/third-party/matchbrackets.js',
    '/playground/third-party/closebrackets.js',
    '/playground/third-party/comment.js',
    '/playground/third-party/lint.js',
    '/playground/third-party/hardwrap.js',
    '/playground/third-party/sublime.js',

    '/playground/codemirror-skip.js',
    '/playground/playground.js',
    '/playground/skip.js',
  ],
  headerIcon: "img/logo.png",
  favicon: "img/favicon.png",
  colors: {
    primaryColor: "#b743b3",
    secondaryColor: "#b743b3",
    prismColor: "hsla(300, 66%, 59%, 0.02)"
  },
  markdownPlugins: [
    function(md) {
      md.renderer.rules.fence_custom.runnable = function(tokens, idx, options, env, instance) {
        var parts = tokens[idx].content.split('// --');
        var includes = `
          <div class="runnable-block">
            <textarea class="code">${parts[0].trim()}</textarea>
            <button class="playground-button run" title="Press ctrl-enter to run"><div class="playground-spin"></div><span class="playground-play">&#9654;</span> Run</button>
            <div class="output"></div>
          </div>
          <script src="/playground/playground.js"></script>
          <script>
            (function() {
              function last(e) { return e[e.length - 1]; }
              setupEditor({
                textarea: last(document.getElementsByClassName('code')),
                output: last(document.getElementsByClassName('output')),
                run: last(document.getElementsByClassName('run')),
              }, {}, '/playground/', '${(parts[1] || '').trim().replace(/[\\']/g, '\\$&').replace(/\n/g, '\\n')}');
            })();
          </script>
        `;
        return includes + '';
      };
    }
  ],
  highlight: {
    defaultLang: 'skip',
    hljs: function(hljs) {
      hljs.registerLanguage('skip', function(hljs) {
        var IDENT_RE = '[A-Za-z_][0-9A-Za-z$_]*';
        var KEYWORDS = {
          keyword: [
            'alias',
            'as',
            'async',
            'await',
            'base',
            'break',
            'catch',
            'children',
            'class',
            'concurrent',
            'const',
            'continue',
            'deferred',
            'do',
            'else',
            'end',
            'extends',
            'extension',
            'final',
            'foreach',
            'from',
            'frozen',
            'fun',
            'if',
            'in',
            'match',
            'memoized',
            'module',
            'module',
            'mutable',
            'native',
            'overridable',
            'private',
            'protected',
            'readonly',
            'static',
            'throw',
            'trait',
            'try',
            'type',
            'untracked',
            'uses',
            'value',
            'when',
            'while',
            'with',
          ].join(' '),
          literal: [
            'false',
            'true',
            'void',
          ].join(' '),
          built_in: [
            'arguments',
            'debug_break',
            'debug',
            'exit',
            'getcwd',
            'internalExit',
            'invariant_violation',
            'invariant',
            'now',
            'open_file',
            'print_error',
            'print_last_exception_stack_trace_and_exit',
            'print_raw',
            'print_stack_trace',
            'print_string',
            'profile_pause',
            'profile_resume',
            'profile_start',
            'profile_stop',
            'read_stdin',
            'string_to_file',
          ].join(' '),
        };

        var TYPE = {
          className: 'type',
          begin: /\b[A-Z][a-zA-Z0-9_]*\b/,
          relevance: 0
        };

        var PARAMS_CONTAINS = [
          hljs.APOS_STRING_MODE,
          hljs.QUOTE_STRING_MODE,
          hljs.C_NUMBER_MODE,
          hljs.REGEXP_MODE,
          hljs.C_BLOCK_COMMENT_MODE,
          hljs.C_LINE_COMMENT_MODE,
          TYPE
        ];

        return {
          keywords: KEYWORDS,
          contains: [
            hljs.APOS_STRING_MODE,
            hljs.QUOTE_STRING_MODE,
            hljs.C_LINE_COMMENT_MODE,
            hljs.C_BLOCK_COMMENT_MODE,
            hljs.C_NUMBER_MODE,
            TYPE,
            { // "value" container
              begin: '(' + hljs.RE_STARTERS_RE + '|\\b(case|return|throw)\\b)\\s*',
              keywords: 'return throw case',
              contains: [
                hljs.C_LINE_COMMENT_MODE,
                hljs.C_BLOCK_COMMENT_MODE,
                hljs.REGEXP_MODE,
                {
                  className: 'function',
                  begin: '(\\(.*?\\)|' + IDENT_RE + ')\\s*[-~]>', returnBegin: true,
                  end: '\\s*[-~]>',
                  contains: [
                    {
                      className: 'params',
                      variants: [
                        {
                          begin: IDENT_RE
                        },
                        {
                          begin: /\(\s*\)/,
                        },
                        {
                          begin: /\(/, end: /\)/,
                          excludeBegin: true, excludeEnd: true,
                          keywords: KEYWORDS,
                          contains: PARAMS_CONTAINS
                        }
                      ]
                    }
                  ]
                },
              ],
              relevance: 0
            },
            {
              className: 'function',
              beginKeywords: 'fun', end: /\{/, excludeEnd: true,
              contains: [
                TYPE,
                hljs.inherit(hljs.TITLE_MODE, {begin: IDENT_RE}),
                {
                  className: 'params',
                  begin: /\(/, end: /\)/,
                  excludeBegin: true,
                  excludeEnd: true,
                  contains: PARAMS_CONTAINS
                }
              ],
            },
            hljs.METHOD_GUARD,
          ],
        };
      });
    }
  },
  separateCss: ["specification/spec.css"],
  algolia: {
    apiKey: 'dfe1f25f0395cc6e7e74dd65fc4f8620',
    indexName: 'skip',
  },
  layouts: {
    'stdlib': require('./core/StdlibLayout.js')
  }
};

module.exports = siteConfig;
