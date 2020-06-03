// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var cs = chrome.contentSettings;

function expect(expected, message) {
  return chrome.test.callbackPass(function(value) {
    chrome.test.assertEq(expected, value, message);
  });
}

chrome.test.runTests([function setAndCheckContentSettings() {
  cs['plugins'].set({
    'primaryPattern': 'http://drive.google.com:443/*',
    'secondaryPattern': '<all_urls>',
    'setting': 'allow'
  });
  cs['plugins'].set({
    'primaryPattern': 'http://*.google.com:443/*',
    'secondaryPattern': '<all_urls>',
    'setting': 'allow'
  });
  cs['plugins'].get(
      {'primaryUrl': 'http://drive.google.com:443/'},
      expect(
          {'setting': 'allow'},
          'Flash should be allowed on this ' +
              'page'));
  cs['plugins'].get(
      {'primaryUrl': 'http://mail.google.com:443/'},
      expect(
          {'setting': 'block'},
          'Flash should be blocked on this page' +
              ' because wildcards are not allowed'));
}]);