// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

function createAdFrame(url, name, sbox_attr) {
  let frame = document.createElement('iframe');
  frame.name = name;
  frame.id = name;
  frame.src = url;
  if (sbox_attr !== undefined) {
    frame.sandbox = sbox_attr;
  }
  document.body.appendChild(frame);
}

function windowOpenFromAdScript() {
  window.open();
}

async function createDocWrittenAdFrame(name, base_url) {
  let doc_body = await fetch('frame_factory.html');
  let doc_text = await doc_body.text();

  let frame = document.createElement('iframe');
  frame.name = name;
  document.body.appendChild(frame);

  frame.contentDocument.open();
  frame.onload = function() {
    window.domAutomationController.send(true);
  }
  frame.contentDocument.write(doc_text);
  frame.contentDocument.close();
}

function createAdFrameWithDocWriteAbortedLoad(name) {
  let frame = document.createElement('iframe');
  frame.name = name;

  // slow takes 100 seconds to load, plenty of time to overwrite the
  // provisional load.
  frame.src = '/slow?100';
  document.body.appendChild(frame);
  frame.contentDocument.open();
  frame.contentDocument.write('<html><head></head><body></body></html>');
  frame.contentDocument.close();
}

function createAdFrameWithWindowStopAbortedLoad(name) {
  let frame = document.createElement('iframe');
  frame.name = name;

  // slow takes 100 seconds to load, plenty of time to overwrite the
  // provisional load.
  frame.src = '/slow?100';
  document.body.appendChild(frame);
  frame.contentWindow.stop();
}
