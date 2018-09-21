#!/usr/bin/env node

# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

const fs = require('fs');
const child_process = require('child_process');
const rpc = require('vscode-jsonrpc');
const createPatch = require('rfc6902').createPatch;

function isEqual(value1, value2) {
  return createPatch(value1, value2).length === 0;
}

const logMessages = false;
function log(message) {
  if (logMessages) {
    console.log(message);
    console.log('\n');
  }
}
function logJson(prefix, json) {
  log(prefix + JSON.stringify(json));
}

// Reads a script generated from log_to_script.
// Starts an LSP process, and replays the script against that process.
let update = false;
if (process.argv[2] === '--update') {
  console.error('run_lsp_script: Updating baseline');
  update = true;
  process.argv.shift();
}
let newBaseline = [];

if (process.argv.length < 5) {
  console.error('Usage: run_lsp_script.sh [--update] script-name root-source-dir skip-lsp-path');
  process.exit(1);
}

const scriptName = process.argv[2];
const rootSourceDir = process.argv[3];
const skipLspPath = process.argv[4];

const currentDirPlaceholder = '%CURRENT_DIR%';

// Convert a JS string for use in regexp
function escapeRegExp(str) {
  return str.replace(/[\-\[\]\/\{\}\(\)\*\+\?\.\\\^\$\|]/g, "\\$&");
}

// Read the script, and localize it to the root directory.
function readScript() {
  const scriptContents = fs.readFileSync(scriptName, 'utf8').replace(
    new RegExp(currentDirPlaceholder, 'g'),
    rootSourceDir,
  );
  return JSON.parse(scriptContents);
}

async function main() {
  const childProcess = child_process.spawn(skipLspPath, ['--test-mode']);
  /* DEBUG
  childProcess.stdout.on('data', data => {
    console.error('\n\nstdout: ' + data.toString());
  });
  childProcess.stderr.on('data', data => {
    console.error('\n\nstderr: ' + data.toString());
  });
  */

  try {
    try {
      // Use stdin and stdout for communication:
      const writer = new rpc.StreamMessageWriter(childProcess.stdin);
      const reader = new rpc.StreamMessageReader(childProcess.stdout);

      await processEntries(reader, writer, readScript());

      if (update) {
        // newBaseline contains an array of messages.

        // Repoint the messages to the current working directory.
        const result = JSON.stringify(newBaseline, null, 2).
          replace(
            new RegExp(escapeRegExp(rootSourceDir), 'g'),
            currentDirPlaceholder,
          );

        // and overwrite the old baseline.
        fs.writeFileSync(scriptName, result);
      }
    } finally {
      childProcess.kill();
    }
  } catch (e) {
    if (!(e instanceof TestFailure)) {
      console.error('Uncaught exception: ' + e.toString());
      process.exit(3);
    } else {
      console.error('FAILED');
      process.exit(2);
    }
  }
}

function createTimeoutPromise(timeout) {
  return new Promise(function(resolve, reject) {
    setTimeout(resolve, timeout, undefined);
  });
}

// Reader can callback multiple times without giving up the tick.
// So we need to Q messages in that case.
class ReaderListener {
  constructor(reader) {
    this.reader = reader;
    this.messages = [];
    this.waiters = [];

    reader.listen(this.onMessage.bind(this));
    reader.onError(this.onError.bind(this));
  }

  onMessage(message) {
    logJson('recieved: ', message);
    if (this.waiters.length > 0) {
      this.waiters.shift()[0](message);
    } else {
      this.messages.push(Promise.resolve(message));
    }
  }

  onError(error) {
    logJson('recieved: ', message);
    if (this.waiters.length > 0) {
      this.waiters.shift()[1](error);
    } else {
      this.messages.push(Promise.reject(error));
    }
  }

  // Returns a promise.
  readMessage() {
    if (this.messages.length > 0) {
      return this.messages.shift();
    } else {
      return new Promise((resolve, reject) => {
        this.waiters.push([resolve, reject]);
      });
    }
  }

  // Returns a Promise which resolves to:
  // undefined on timeout
  // message otherwise
  readMessageWithTimeout() {
    return Promise.race([
      createTimeoutPromise(10000),
      this.readMessage(),
    ]);
  }
}

class TestFailure extends Error {}

// Throw instead of exit, so that finally clauses run.
function failTest() {
  throw new TestFailure();
}

// type EntryKind = 'send' | 'receive';
// type Entry = {
//    kind: EntryKind,
//    contents: LSPMessage,
//    ?elapsedTime: number,
//    ?deltaTime: number,
// };
// entries: Entry[];
async function processEntries(reader, writer, entries) {
  const readerListener = new ReaderListener(reader);

  let index = 0;

process_entries:
  while (index < entries.length) {
    const entry = entries[index];
    const rpcMessage = entry.contents;
    const id = rpcMessage.id;
    const params = rpcMessage.params;
    const isNotification = id == null;

    switch (entry.kind) {
    case 'receive':
      // save file
      if (rpcMessage.method === 'textDocument/didSave') {
        fs.writeFileSync(
          rpcMessage.params.textDocument.uri.replace('file://', ''),
          rpcMessage.params.text);
      }
      logJson('sent: ', rpcMessage);
      writer.write(rpcMessage);

      // append this message, and subsequent received messages to the expected
      // results, until a "skip_lsp: Done Processing" telemetry message is received.
      if (update) {
        newBaseline.push(entry);
        // DEBUG
        // console.error('Added received to baseline: ' + JSON.stringify(entry));

        // Don't expect a response to the shutdown method. skip_lsp proactively
        // exits, rather than waits for an 'exit' request which often never comes.
        if (rpcMessage.method === 'shutdown') {
          break process_entries;
        }

        while(true) {
          const message = await readerListener.readMessageWithTimeout();
          if (message === undefined) {
            console.error(`Timeout expecting telemetry after ${index}:`);
            console.error(JSON.stringify(rpcMessage));
            failTest();
          } else {
            sentMessage = {
              kind: 'send',
              contents: message,
            };
            newBaseline.push(sentMessage);
            if (message.method === 'telemetry/event' && message.params === 'skip_lsp: Done Processing') {
              break;
            }
            // DEBUG
            // console.error('Added sent to baseline: ' + JSON.stringify(sentMessage));
          }
        }
      }
      break;
    case 'send':
      if (!update) {
        const message = await readerListener.readMessageWithTimeout();
        if (message === undefined) {
          console.error(`Timeout expecting entry ${index}:`);
          console.error(JSON.stringify(rpcMessage));
          failTest();
        } else if (!isEqual(message, rpcMessage)) {
          console.error('Received unexpected message:');
          console.error('Expected: ' + JSON.stringify(rpcMessage));
          console.error('Actual: ' + JSON.stringify(message));
          console.error('Diff: ' + JSON.stringify(createPatch(message, rpcMessage)));
          failTest();
        }
      }
    }

    index += 1;
  }

  console.log('PASS');
}

main();
