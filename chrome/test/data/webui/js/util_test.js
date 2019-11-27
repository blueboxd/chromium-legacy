// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$, quoteString} from 'chrome://resources/js/util.m.js';

suite('UtilModuleTest', function() {
  test('quote string', function() {
    // Basic cases.
    assertEquals('\"test\"', quoteString('"test"'));
    assertEquals('\\!\\?', quoteString('!?'));
    assertEquals(
        '\\(\\._\\.\\) \\( \\:l \\) \\(\\.-\\.\\)',
        quoteString('(._.) ( :l ) (.-.)'));

    // Using the output as a regex.
    var re = new RegExp(quoteString('"hello"'), 'gim');
    var match = re.exec('She said "Hello" loudly');
    assertEquals(9, match.index);

    re = new RegExp(quoteString('Hello, .*'), 'gim');
    match = re.exec('Hello, world');
    assertEquals(null, match);
  });
});
