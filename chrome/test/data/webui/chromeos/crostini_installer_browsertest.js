// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Test suite for the Crostini Installer page.
 */
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);
GEN('#include "chromeos/constants/chromeos_features.h"');

function CrostiniInstallerBrowserTest() {}

CrostiniInstallerBrowserTest.prototype = {
  __proto__: PolymerTest.prototype,

  browsePreload: 'chrome://crostini-installer',

  featureList: {enabled: ['chromeos::features::kCrostiniWebUIInstaller']},
};

TEST_F('CrostiniInstallerBrowserTest', 'All', function() {
  suite('<crostini-installer-app>', () => {
    test('install', done => {
      // TODO(lxj)
      done();
    });
  });

  mocha.run();
});
