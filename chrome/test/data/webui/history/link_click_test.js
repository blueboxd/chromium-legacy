// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('listenForPrivilegedLinkClicks unit test', function() {
  test('click handler', function() {
    history.listenForPrivilegedLinkClicks();
    document.body.innerHTML = `
      <a id="file" href="file:///path/to/file">File</a>
      <a id="chrome" href="about:chrome">Chrome</a>
      <a href="about:blank"><b id="blank">Click me</b></a>
    `;

    let clickArgs = null;
    const oldSend = chrome.send;
    chrome.send = function(message, args) {
      assertEquals('navigateToUrl', message);
      clickArgs = args;
    };
    $('file').click();
    assertEquals('file:///path/to/file', clickArgs[0]);
    $('chrome').click();
    assertEquals('about:chrome', clickArgs[0]);
    $('blank').click();
    assertEquals('about:blank', clickArgs[0]);
    chrome.send = oldSend;
  });
});
