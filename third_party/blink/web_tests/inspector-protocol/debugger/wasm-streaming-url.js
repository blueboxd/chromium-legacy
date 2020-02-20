// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function instantiate() {
  const magicModuleHeader = [0x00, 0x61, 0x73, 0x6d], moduleVersion = [0x01, 0x00, 0x00, 0x00];
  const wasm = Uint8Array.from([...magicModuleHeader, ...moduleVersion]);
  let b = new Blob([wasm.buffer], {type: 'application/wasm'});
  let bURL =  URL.createObjectURL(b);
  fetch(bURL)
  .then(WebAssembly.instantiateStreaming)
  .catch(console.error);
  return bURL.toString();
}

(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Test script URL for wasm streaming compilation.');

  await dp.Debugger.enable();
  testRunner.log('Did enable debugger');

  testRunner.log('Instantiating.');
  var response = await dp.Runtime
      .evaluate({expression: '(' + instantiate + ')()'});
  testRunner.log('Waiting for wasm script');
  const {params: wasm_script} = await dp.Debugger.onceScriptParsed();

  testRunner.log('URL is preserved: ' + (wasm_script.url == response.result.result.value));
  testRunner.completeTest();
})
