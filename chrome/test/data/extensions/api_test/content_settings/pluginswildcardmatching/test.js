// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var cs = chrome.contentSettings;

function setWildcardedPatterns() {
  cs['plugins'].set({
    'primaryPattern': '<all_urls>',
    'secondaryPattern': '<all_urls>',
    'setting': 'allow'
  });
}

function expect(expected, message) {
  return chrome.test.callbackPass(function(value) {
    chrome.test.assertEq(expected, value, message);
  });
}

chrome.test.runTests([
  function testSchemeWildcardMatching() {
    cs['plugins'].set({
      'primaryPattern': '*://drive.google.com:*/*',
      'secondaryPattern': '<all_urls>',
      'setting': 'allow'
    });
    cs['plugins'].set({
      'primaryPattern': 'https://maps.google.com:443/*',
      'secondaryPattern': '<all_urls>',
      'setting': 'allow'
    });
    cs['plugins'].get(
        {'primaryUrl': 'https://drive.google.com:443/*'},
        expect({'setting': 'allow'}, 'Flash should be blocked on this page'));
    cs['plugins'].get(
        {'primaryUrl': 'http://drive.google.com:80/*'},
        expect({'setting': 'allow'}, 'Flash should be blocked on this page'));
    cs['plugins'].get(
        {'primaryUrl': 'https://maps.google.com:443/*'},
        expect({'setting': 'allow'}, 'Flash should be blocked on this page'));
    cs['plugins'].clear({});
  },
  function testPortWildcardMatching() {
    cs['plugins'].set({
      'primaryPattern': 'https://drive.google.com:*/*',
      'secondaryPattern': '<all_urls>',
      'setting': 'allow'
    });
    cs['plugins'].set({
      'primaryPattern': 'https://maps.google.com:443/*',
      'secondaryPattern': '<all_urls>',
      'setting': 'allow'
    });
    cs['plugins'].get(
        {'primaryUrl': 'https://drive.google.com:100/'},
        expect({'setting': 'allow'}, 'Flash should be allowed on this page'));
    cs['plugins'].get(
        {'primaryUrl': 'https://maps.google.com:443/'},
        expect({'setting': 'allow'}, 'Flash should be allowed on this page'));
    cs['plugins'].clear({});
  },
  function testSubdomainWildcardMatching() {
    cs['plugins'].set({
      'primaryPattern': 'https://*.google.com:443/*',
      'secondaryPattern': '<all_urls>',
      'setting': 'allow'
    });
    cs['plugins'].get(
        {'primaryUrl': 'http://drive.google.com:443/'},
        expect({'setting': 'block'}, 'Flash should be blocked on this page'));
    cs['plugins'].clear({});
  },
  function testFullWilcardMatching() {
    cs['plugins'].set({
      'primaryPattern': '<all_urls>',
      'secondaryPattern': '<all_urls>',
      'setting': 'allow'
    });
    cs['plugins'].set({
      'primaryPattern': 'https://*/*',
      'secondaryPattern': '<all_urls>',
      'setting': 'allow'
    });
    cs['plugins'].set({
      'primaryPattern': '*://*/*',
      'secondaryPattern': '<all_urls>',
      'setting': 'allow'
    });
    cs['plugins'].get(
        {'primaryUrl': 'https://drive.google.com:443/'},
        expect({'setting': 'block'}, 'Flash should be blocked on this page'));
    cs['plugins'].get(
        {'primaryUrl': 'https://example.com:443/'},
        expect({'setting': 'block'}, 'Flash should be blocked on this page'));
    cs['plugins'].clear({});
  },
]);