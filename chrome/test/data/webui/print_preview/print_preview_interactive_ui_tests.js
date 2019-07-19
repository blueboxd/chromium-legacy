// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Print Preview interactive UI tests. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_interactive_ui_test.js']);

const PrintPreviewInteractiveUITest = class extends PolymerInteractiveUITest {
  /** @override */
  get browsePreload() {
    throw 'this is abstract and should be overriden by subclasses';
  }

  /** @override */
  get extraLibraries() {
    return [
      ...super.extraLibraries,
      '//ui/webui/resources/js/assert.js',
    ];
  }

  /** @override */
  get loaderFile() {
    return 'subpage_loader.html';
  }

  // The name of the mocha suite. Should be overridden by subclasses.
  get suiteName() {
    return null;
  }

  // The name of the custom element under test. Should be overridden by
  // subclasses that are not directly loading the URL of a custom element.
  get customElementName() {
    const r = /chrome\:\/\/print\/([a-zA-Z-_]+)\/([a-zA-Z-_]+)\.html/;
    const result = r.exec(this.browsePreload);
    return 'print-preview-' + result[2].replace(/_/gi, '-');
  }

  /** @param {string} testName The name of the test to run. */
  runMochaTest(testName) {
    runMochaTest(this.suiteName, testName);
  }
};

// eslint-disable-next-line no-var
var PrintPreviewPrintHeaderInteractiveTest =
    class extends PrintPreviewInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/header.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//chrome/test/data/webui/settings/test_util.js',
      'print_header_interactive_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return print_header_interactive_test.suiteName;
  }
};

TEST_F(
    'PrintPreviewPrintHeaderInteractiveTest', 'FocusPrintOnReady', function() {
      this.runMochaTest(
          print_header_interactive_test.TestNames.FocusPrintOnReady);
    });

// eslint-disable-next-line no-var
var PrintPreviewButtonStripInteractiveTest =
    class extends PrintPreviewInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/button_strip.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//chrome/test/data/webui/settings/test_util.js',
      'button_strip_interactive_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return button_strip_interactive_test.suiteName;
  }
};

// Web UI interactive tests are flaky on Win10, see https://crbug.com/711256
GEN('#if defined(OS_WIN)');
GEN('#define MAYBE_FocusPrintOnReady DISABLED_FocusPrintOnReady');
GEN('#else');
GEN('#define MAYBE_FocusPrintOnReady FocusPrintOnReady');
GEN('#endif');
TEST_F(
    'PrintPreviewButtonStripInteractiveTest', 'MAYBE_FocusPrintOnReady',
    function() {
      this.runMochaTest(
          button_strip_interactive_test.TestNames.FocusPrintOnReady);
    });

// eslint-disable-next-line no-var
var PrintPreviewDestinationDialogInteractiveTest =
    class extends PrintPreviewInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/destination_settings.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//chrome/test/data/webui/settings/test_util.js',
      '../test_browser_proxy.js',
      'cloud_print_interface_stub.js',
      'native_layer_stub.js',
      'print_preview_test_utils.js',
      'destination_dialog_interactive_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return destination_dialog_interactive_test.suiteName;
  }
};

TEST_F(
    'PrintPreviewDestinationDialogInteractiveTest', 'FocusSearchBox',
    function() {
      this.runMochaTest(
          destination_dialog_interactive_test.TestNames.FocusSearchBox);
    });


TEST_F(
    'PrintPreviewDestinationDialogInteractiveTest', 'FocusSearchBoxOnSignIn',
    function() {
      this.runMochaTest(
          destination_dialog_interactive_test.TestNames.FocusSearchBoxOnSignIn);
    });

TEST_F(
    'PrintPreviewDestinationDialogInteractiveTest', 'EscapeSearchBox',
    function() {
      this.runMochaTest(
          destination_dialog_interactive_test.TestNames.EscapeSearchBox);
    });

// eslint-disable-next-line no-var
var PrintPreviewPagesSettingsTest =
    class extends PrintPreviewInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/pages_settings.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'print_preview_test_utils.js',
      'pages_settings_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return pages_settings_test.suiteName;
  }
};

TEST_F('PrintPreviewPagesSettingsTest', 'ClearInput', function() {
  this.runMochaTest(pages_settings_test.TestNames.ClearInput);
});

TEST_F(
    'PrintPreviewPagesSettingsTest', 'InputNotDisabledOnValidityChange',
    function() {
      this.runMochaTest(
          pages_settings_test.TestNames.InputNotDisabledOnValidityChange);
    });

TEST_F(
    'PrintPreviewPagesSettingsTest', 'EnterOnInputTriggersPrint', function() {
      this.runMochaTest(
          pages_settings_test.TestNames.EnterOnInputTriggersPrint);
    });

// eslint-disable-next-line no-var
var PrintPreviewNumberSettingsSectionInteractiveTest =
    class extends PrintPreviewInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/number_settings_section.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'print_preview_test_utils.js',
      'number_settings_section_interactive_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return number_settings_section_interactive_test.suiteName;
  }
};

TEST_F(
    'PrintPreviewNumberSettingsSectionInteractiveTest', 'BlurResetsEmptyInput',
    function() {
      this.runMochaTest(number_settings_section_interactive_test.TestNames
                            .BlurResetsEmptyInput);
    });

// eslint-disable-next-line no-var
var PrintPreviewScalingSettingsInteractiveTest =
    class extends PrintPreviewInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/scaling_settings.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/util.js',
      '../settings/test_util.js',
      'print_preview_test_utils.js',
      'scaling_settings_interactive_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return scaling_settings_interactive_test.suiteName;
  }
};

// Web UI interactive tests are flaky on Win10, see https://crbug.com/711256
GEN('#if defined(OS_WIN)');
GEN('#define MAYBE_AutoFocusInput DISABLED_InputAutoFocus');
GEN('#else');
GEN('#define MAYBE_AutoFocusInput InputAutoFocus');
GEN('#endif');
TEST_F(
    'PrintPreviewScalingSettingsInteractiveTest', 'MAYBE_AutoFocusInput',
    function() {
      this.runMochaTest(
          scaling_settings_interactive_test.TestNames.AutoFocusInput);
    });
