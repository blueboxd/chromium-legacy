// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function compileStreamingAndSendToWorker() {
  let workerBlob = new Blob([`
  onmessage = function(e) {
    let instance = new WebAssembly.Instance(e.data.module);
  }`],
  {type: 'application/javascript'});
  let worker = new Worker(URL.createObjectURL(workerBlob));

  const magicModuleHeader = [0x00, 0x61, 0x73, 0x6d], moduleVersion = [0x01, 0x00, 0x00, 0x00];
  const wasm = Uint8Array.from([...magicModuleHeader, ...moduleVersion]);
  let b = new Blob([wasm.buffer], {type: 'application/wasm'});
  let bURL =  URL.createObjectURL(b);

  fetch(bURL)
  .then(WebAssembly.compileStreaming)
  .then(module => worker.postMessage(module))
  return bURL.toString();
}

(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Test script URL for wasm streaming compilation.');

  await dp.Debugger.enable();
  testRunner.log('Did enable debugger');
  let url = await session.evaluate('(' + compileStreamingAndSendToWorker + ')()');

  // Wait for script creation in the main thread.
  let {params: main_script} = await dp.Debugger.onceScriptParsed();
  testRunner.log('URL is preserved in the main thread: ' + (main_script.url == url));

  // Wait for worker.
  const attachedPromise = dp.Target.onceAttachedToTarget();
  await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: false,
                           flatten: true});
  const event = await attachedPromise;
  const childSession = session.createChild(event.params.sessionId);
  testRunner.log('Worker created');

  // Wait for script creation in the worker.
  await childSession.protocol.Debugger.enable({});
  let {params: worker_script} = await childSession.protocol.Debugger.onceScriptParsed();
  testRunner.log('URL is preserved in the worker: ' + (worker_script.url == url));
  testRunner.completeTest();
})
