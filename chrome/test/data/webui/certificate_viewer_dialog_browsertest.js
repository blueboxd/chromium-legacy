// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Test fixture for generated tests.
 * @extends {testing.Test}
 */
function CertificateViewerUITest() {}

CertificateViewerUITest.prototype = {
  __proto__: testing.Test.prototype,

  /** @override */
  isAsync: true,

  /** @override */
  extraLibraries: [
    '//third_party/mocha/mocha.js',
    '//chrome/test/data/webui/mocha_adapter.js',
    'test_util.js',
    'certificate_viewer_dialog_test.js',
  ],

  /**
   * Define the C++ fixture class and include it.
   * @type {?string}
   * @override
   */
  typedefCppFixture: 'CertificateViewerUITest',

  /**
   * Show the certificate viewer dialog.
   */
  testGenPreamble: function() {
    GEN('ShowCertificateViewer();');
  },
};


// Include the bulk of c++ code.
// Certificate viewer UI tests are disabled on platforms with native certificate
// viewers.
GEN('#include "chrome/test/data/webui/certificate_viewer_ui_test-inl.h"');
GEN('');

// Constructors and destructors must be provided in .cc to prevent clang errors.
GEN('CertificateViewerUITest::CertificateViewerUITest() {}');
GEN('CertificateViewerUITest::~CertificateViewerUITest() {}');

TEST_F('CertificateViewerUITest', 'DialogURL', function() {
  mocha.grep('DialogURL').run();
});

TEST_F('CertificateViewerUITest', 'CommonName', function() {
  mocha.grep('CommonName').run();
});

TEST_F('CertificateViewerUITest', 'Details', function() {
  mocha.grep('Details').run();
});
