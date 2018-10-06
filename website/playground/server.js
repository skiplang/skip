/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

var fs = require('fs');
var path = require('path');

var rootPath = path.resolve(__dirname, '../..');
var buildPath = path.resolve(rootPath, 'build');
var binPath = path.resolve(buildPath, 'bin');
var runtimePath = path.resolve(rootPath, 'tests/runtime');
var preludePath = path.resolve(rootPath, 'tests/runtime/prelude');
var runtimeToolsPath = path.resolve(runtimePath, 'tools');
var toolsPath = path.resolve(rootPath, 'tools');
var tmpPath = path.resolve(__dirname, 'tmp');
var playgroundID = 0;

if (!fs.existsSync(path.resolve(binPath, 'skip_to_js'))) {
  console.error(
    'Missing ../../build/bin/skip_to_js, please run `cmake ..`' +
    ' and `ninja skip_to_native skip_to_js` in `../../build`'
  );
  return;
}

if (!fs.existsSync(path.resolve(__dirname, 'node_modules'))) {
  console.error('Missing node_modules/, please run `yarn`');
  return;
}

if (!fs.existsSync(path.resolve(__dirname, 'static/skip.js'))) {
  console.error('Missing static/skip.js, please run `yarn build`');
  return;
}

var express = require('express');
var app = express();
var bodyParser = require('body-parser');
var compression = require('compression');
var spawnSync = require('child_process').spawnSync;
var execFile = require('child_process').execFile;
var glob = require('glob');
var cors = require('cors');

app.use(compression());
app.use(express.static('static'));
app.use(express.static('tmp'));
app.use(bodyParser.text());
app.use(cors());

function normalizePath(str) {
  return str
    .split(tmpPath + '/').join('')
    .split(rootPath + '/').join('')
    .replace(/playground[0-9]+\.sk/g, 'playground.sk');
}

function writePlayground(req) {
  try { fs.mkdirSync(tmpPath); } catch(e) {}
  var playgroundPath = path.resolve(tmpPath, 'playground' + ((playgroundID++) % 30) + '.sk');
  fs.writeFileSync(playgroundPath, req.body);
  return playgroundPath;
}

var outstandingCount = 0;
function run(res, cmd, args, cb, env) {
  if (outstandingCount === 10) {
    sendError(res, 'Too many connections, please try again later');
    return;
  }
  outstandingCount++;
  execFile(
    cmd,
    args,
    {
      timeout: 30 * 1000,
      maxBuffer: 2 * 1024 * 1024,
      cwd: rootPath,
      env,
    },
    function(error, stdout, stderr) {
      outstandingCount--;
      if (error && error.code === 'ENOENT') {
        sendError(res, 'File not found ' + cmd);
      } else if (error) {
        sendError(res, stdout.toString() + normalizePath(stderr.toString()));
      } else {
        cb(stdout.toString(), stderr.toString());
      }
    }
  );
}

function sendError(res, content) {
  // 4xx error messages display an annoying entry in the chrome console
  // that you can't disable :(
  // https://bugs.chromium.org/p/chromium/issues/detail?id=124534
  res.status(201);
  res.send(content);
}

function catchErrors(f) {
  return function(req, res) {
    try {
      f(req, res);
    } catch(e) {
      sendError(res, e.toString());
    }
  }
}

app.post('/skip_to_native', catchErrors(function(req, res) {
  var playgroundPath = writePlayground(req);
  var outPath = path.resolve(tmpPath, 'sk.ll');
  var args = [
    playgroundPath,
    preludePath,
    '-O1',
    '--json',
    '--disasm-file', playgroundPath,
    '--disasm-all',
    '--filter-disasm',
    '--output', outPath,
    '--via-backend', binPath,
    '--preamble', path.resolve(buildPath, 'tests/runtime/native/lib/preamble.ll'),
  ];
  run(
    res,
    path.resolve(runtimeToolsPath, 'skip_to_native'),
    args,
    function(stdout) {
      var disasm = fs.readFileSync(outPath, 'utf8');
      res.send(stdout + disasm);
    }
  );
}));

app.post('/skip_to_js', catchErrors(function(req, res) {
  var playgroundPath = writePlayground(req);
  var outputPath = path.resolve(tmpPath, path.basename(playgroundPath).replace(/.sk$/, '.js'));
  run(
    res,
    path.resolve(binPath, 'skip_to_js'),
    [
      '--output', outputPath,
    ]
      .concat('filter' in req.query ? ['--filter', playgroundPath] : [])
      .concat([preludePath, playgroundPath]),
    function(stdout) {
      var output = fs.readFileSync(outputPath, 'utf8');
      if (stdout !== '') {
        output = 'log(' + JSON.stringify(stdout) + ');' + output;
      }
      res.send(output);
    }
  );
}));

app.post('/skip_to_ast', catchErrors(function(req, res) {
  var playgroundPath = writePlayground(req);
  run(
    res,
    path.resolve(binPath, 'skip_to_ast'),
    [playgroundPath],
    function(stdout, stderr) {
      res.send(stderr);
    }
  );
}));

app.post('/skip_to_parsetree', catchErrors(function(req, res) {
  var playgroundPath = writePlayground(req);
  run(
    res,
    path.resolve(binPath, 'skip_to_parsetree'),
    [playgroundPath],
    function(stdout, stderr) {
      res.send(stderr);
    }
  );
}));

app.post('/skip_printer', catchErrors(function(req, res) {
  var playgroundPath = writePlayground(req);
  run(
    res,
    path.resolve(binPath, 'skip_printer'),
    [playgroundPath],
    function(stdout) {
      res.send(stdout);
    }
  );
}));

app.post('/skip_search', catchErrors(function(req, res) {
  var playgroundPath = writePlayground(req);
  // options is optional
  glob("**/*.sk", {cwd: rootPath}, function (err, files) {
    if (err) {
      sendError(res, err);
    } else {
      run(
        res,
        path.resolve(binPath, 'skip_search'),
        [fs.readFileSync(playgroundPath, 'utf8')].concat(
          files.filter(file =>
            file.indexOf('tests/') === -1 &&
            file.indexOf('__tests__/') === -1 &&
            file.indexOf('playground/') === -1 &&
            file.indexOf('build/') === -1
          )
        ),
        function(stdout) {
          res.send(stdout);
        }
      );
    }
  })
}));

app.listen('8081');
console.log('Open http://localhost:8081');
