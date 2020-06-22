// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
var header = document.getElementById('untrusted-title');
header.textContent = 'Untrusted Telemetry Extension';

// For testing purposes: notify the parent window the iframe has been embedded
// successfully.
window.addEventListener('message', event => {
  if (event.origin.startsWith('chrome://telemetry-extension')) {
    let data = /** @type {string} */ (event.data);
    if (data === 'runWebWorker') {
      runWebWorker();
    } else if (data === 'hello') {
      window.parent.postMessage(
          {'success': true}, 'chrome://telemetry-extension');
    }
  }
});

/**
 * Starts dedicated worker.
 */
function runWebWorker() {
  if (!window.Worker) {
    console.error('Error: Worker is not supported!');
    return;
  }

  let worker = new Worker('untrusted_worker.js');

  console.debug('Starting Worker...', worker);

  /**
   * Registers onmessage event handler.
   * @param {MessageEvent} event Incoming message event.
   */
  worker.onmessage = function(event) {
    let data = /** @type {string} */ (event.data);

    console.debug('Message received from worker:', data);

    window.parent.postMessage(
        {'message': data}, 'chrome://telemetry-extension');
  };

  worker.postMessage('WebWorker');
}
