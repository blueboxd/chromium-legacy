// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #clang-format off
import 'chrome://settings/settings.js';
// #clang-format on

suite('Settings idle load tests', function() {
  setup(function() {
    document.body.innerHTML = `
      <settings-idle-load id="idleTemplate"
          url="chrome://resources/html/polymer.html">
        <template>
          <div></div>
        </template>
      </settings-idle-load>
    `;
    // The div should not be initially accessible.
    assertFalse(!!document.body.querySelector('div'));
  });

  test('stamps after get()', function() {
    // Calling get() will force stamping without waiting for idle time.
    return document.getElementById('idleTemplate').get().then(function(inner) {
      assertEquals('DIV', inner.nodeName);
      assertEquals(inner, document.body.querySelector('div'));
    });
  });

  test('stamps after idle', function(done) {
    requestIdleCallback(function() {
      // After JS calls idle-callbacks, this should be accessible.
      assertTrue(!!document.body.querySelector('div'));
      done();
    });
  });
});
