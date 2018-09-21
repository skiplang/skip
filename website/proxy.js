/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

var proxy = require('express-http-proxy');

var app = require('express')();

app.use('/playground', proxy('localhost:8081'));
app.use('', proxy('localhost:8082'));

app.listen('8080');
console.log('http://localhost:8080')
