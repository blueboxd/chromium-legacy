// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Print Preview tests. */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

const PrintPreviewTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/';
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
var PrintPreviewAppTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/app.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      '../test_browser_proxy.js',
      'cloud_print_interface_stub.js',
      'native_layer_stub.js',
      'plugin_stub.js',
      'print_preview_test_utils.js',
      'print_preview_app_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return print_preview_app_test.suiteName;
  }
};

TEST_F('PrintPreviewAppTest', 'PrintToGoogleDrive', function() {
  this.runMochaTest(print_preview_app_test.TestNames.PrintToGoogleDrive);
});

TEST_F('PrintPreviewAppTest', 'PrintPresets', function() {
  this.runMochaTest(print_preview_app_test.TestNames.PrintPresets);
});

// eslint-disable-next-line no-var
var PrintPreviewSidebarTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/sidebar.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      '../test_browser_proxy.js',
      'cloud_print_interface_stub.js',
      'native_layer_stub.js',
      'print_preview_test_utils.js',
      'print_preview_sidebar_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return print_preview_sidebar_test.suiteName;
  }
};

TEST_F(
    'PrintPreviewSidebarTest', 'SettingsSectionsVisibilityChange', function() {
      this.runMochaTest(print_preview_sidebar_test.TestNames
                            .SettingsSectionsVisibilityChange);
    });

// eslint-disable-next-line no-var
var PrintPreviewPagesSettingsTest = class extends PrintPreviewTest {
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

TEST_F('PrintPreviewPagesSettingsTest', 'PagesDropdown', function() {
  this.runMochaTest(pages_settings_test.TestNames.PagesDropdown);
});

TEST_F('PrintPreviewPagesSettingsTest', 'ValidPageRanges', function() {
  this.runMochaTest(pages_settings_test.TestNames.ValidPageRanges);
});

TEST_F('PrintPreviewPagesSettingsTest', 'InvalidPageRanges', function() {
  this.runMochaTest(pages_settings_test.TestNames.InvalidPageRanges);
});

TEST_F('PrintPreviewPagesSettingsTest', 'NupChangesPages', function() {
  this.runMochaTest(pages_settings_test.TestNames.NupChangesPages);
});

// eslint-disable-next-line no-var
var PrintPreviewPolicyTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/app.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      '../test_browser_proxy.js',
      'native_layer_stub.js',
      'plugin_stub.js',
      'print_preview_test_utils.js',
      'policy_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return policy_tests.suiteName;
  }
};

TEST_F('PrintPreviewPolicyTest', 'EnableHeaderFooterByPref', function() {
  this.runMochaTest(policy_tests.TestNames.EnableHeaderFooterByPref);
});

TEST_F('PrintPreviewPolicyTest', 'DisableHeaderFooterByPref', function() {
  this.runMochaTest(policy_tests.TestNames.DisableHeaderFooterByPref);
});

TEST_F('PrintPreviewPolicyTest', 'EnableHeaderFooterByPolicy', function() {
  this.runMochaTest(policy_tests.TestNames.EnableHeaderFooterByPolicy);
});

TEST_F('PrintPreviewPolicyTest', 'DisableHeaderFooterByPolicy', function() {
  this.runMochaTest(policy_tests.TestNames.DisableHeaderFooterByPolicy);
});

// eslint-disable-next-line no-var
var PrintPreviewSettingsSelectTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/settings_select.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'print_preview_test_utils.js',
      'settings_select_test.js',
    ]);
  }
};

TEST_F('PrintPreviewSettingsSelectTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var PrintPreviewSelectBehaviorTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/select_behavior.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'select_behavior_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return select_behavior_test.suiteName;
  }

  /** @override */
  get customElementName() {
    // This test is loading a behavior, not an element.
    return null;
  }
};

TEST_F('PrintPreviewSelectBehaviorTest', 'CallProcessSelectChange', function() {
  this.runMochaTest(select_behavior_test.TestNames.CallProcessSelectChange);
});

// eslint-disable-next-line no-var
var PrintPreviewNumberSettingsSectionTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/number_settings_section.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'number_settings_section_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return number_settings_section_test.suiteName;
  }
};

TEST_F(
    'PrintPreviewNumberSettingsSectionTest', 'BlocksInvalidKeys', function() {
      this.runMochaTest(
          number_settings_section_test.TestNames.BlocksInvalidKeys);
    });

// eslint-disable-next-line no-var
var PrintPreviewRestoreStateTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/app.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../test_browser_proxy.js',
      'native_layer_stub.js',
      'plugin_stub.js',
      'print_preview_test_utils.js',
      'restore_state_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return restore_state_test.suiteName;
  }
};

TEST_F('PrintPreviewRestoreStateTest', 'RestoreTrueValues', function() {
  this.runMochaTest(restore_state_test.TestNames.RestoreTrueValues);
});

TEST_F('PrintPreviewRestoreStateTest', 'RestoreFalseValues', function() {
  this.runMochaTest(restore_state_test.TestNames.RestoreFalseValues);
});

TEST_F('PrintPreviewRestoreStateTest', 'SaveValues', function() {
  this.runMochaTest(restore_state_test.TestNames.SaveValues);
});

// eslint-disable-next-line no-var
var PrintPreviewModelTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/data/model.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'print_preview_test_utils.js',
      'model_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return model_test.suiteName;
  }
};

TEST_F('PrintPreviewModelTest', 'SetStickySettings', function() {
  this.runMochaTest(model_test.TestNames.SetStickySettings);
});

TEST_F('PrintPreviewModelTest', 'SetPolicySettings', function() {
  this.runMochaTest(model_test.TestNames.SetPolicySettings);
});

TEST_F('PrintPreviewModelTest', 'GetPrintTicket', function() {
  this.runMochaTest(model_test.TestNames.GetPrintTicket);
});

TEST_F('PrintPreviewModelTest', 'GetCloudPrintTicket', function() {
  this.runMochaTest(model_test.TestNames.GetCloudPrintTicket);
});

TEST_F('PrintPreviewModelTest', 'ChangeDestination', function() {
  this.runMochaTest(model_test.TestNames.ChangeDestination);
});

// eslint-disable-next-line no-var
var PrintPreviewModelSettingsAvailabilityTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/data/model.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'print_preview_test_utils.js',
      'model_settings_availability_test.js',
    ]);
  }
};

TEST_F('PrintPreviewModelSettingsAvailabilityTest', 'All', function() {
  mocha.run();
});

GEN('#if defined(OS_CHROMEOS)');
// eslint-disable-next-line no-var
var PrintPreviewModelSettingsPolicyTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/data/model.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'print_preview_test_utils.js',
      'model_settings_policy_test.js',
    ]);
  }
};

TEST_F('PrintPreviewModelSettingsPolicyTest', 'All', function() {
  mocha.run();
});
GEN('#endif');

// eslint-disable-next-line no-var
var PrintPreviewPreviewGenerationTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/app.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../test_browser_proxy.js',
      'native_layer_stub.js',
      'plugin_stub.js',
      'print_preview_test_utils.js',
      'preview_generation_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return preview_generation_test.suiteName;
  }
};

TEST_F('PrintPreviewPreviewGenerationTest', 'Color', function() {
  this.runMochaTest(preview_generation_test.TestNames.Color);
});

TEST_F('PrintPreviewPreviewGenerationTest', 'CssBackground', function() {
  this.runMochaTest(preview_generation_test.TestNames.CssBackground);
});

TEST_F('PrintPreviewPreviewGenerationTest', 'FitToPage', function() {
  this.runMochaTest(preview_generation_test.TestNames.FitToPage);
});

TEST_F('PrintPreviewPreviewGenerationTest', 'HeaderFooter', function() {
  this.runMochaTest(preview_generation_test.TestNames.HeaderFooter);
});

TEST_F('PrintPreviewPreviewGenerationTest', 'Layout', function() {
  this.runMochaTest(preview_generation_test.TestNames.Layout);
});

TEST_F('PrintPreviewPreviewGenerationTest', 'Margins', function() {
  this.runMochaTest(preview_generation_test.TestNames.Margins);
});

TEST_F('PrintPreviewPreviewGenerationTest', 'CustomMargins', function() {
  this.runMochaTest(preview_generation_test.TestNames.CustomMargins);
});

TEST_F('PrintPreviewPreviewGenerationTest', 'MediaSize', function() {
  this.runMochaTest(preview_generation_test.TestNames.MediaSize);
});

TEST_F('PrintPreviewPreviewGenerationTest', 'PageRange', function() {
  this.runMochaTest(preview_generation_test.TestNames.PageRange);
});

TEST_F('PrintPreviewPreviewGenerationTest', 'SelectionOnly', function() {
  this.runMochaTest(preview_generation_test.TestNames.SelectionOnly);
});

TEST_F('PrintPreviewPreviewGenerationTest', 'PagesPerSheet', function() {
  this.runMochaTest(preview_generation_test.TestNames.PagesPerSheet);
});

TEST_F('PrintPreviewPreviewGenerationTest', 'Scaling', function() {
  this.runMochaTest(preview_generation_test.TestNames.Scaling);
});

GEN('#if !defined(OS_WIN) && !defined(OS_MACOSX)');
TEST_F('PrintPreviewPreviewGenerationTest', 'Rasterize', function() {
  this.runMochaTest(preview_generation_test.TestNames.Rasterize);
});
GEN('#endif');

TEST_F('PrintPreviewPreviewGenerationTest', 'Destination', function() {
  this.runMochaTest(preview_generation_test.TestNames.Destination);
});

TEST_F(
    'PrintPreviewPreviewGenerationTest', 'ChangeMarginsByPagesPerSheet',
    function() {
      this.runMochaTest(
          preview_generation_test.TestNames.ChangeMarginsByPagesPerSheet);
    });

TEST_F(
    'PrintPreviewPreviewGenerationTest', 'ZeroDefaultMarginsClearsHeaderFooter',
    function() {
      this.runMochaTest(preview_generation_test.TestNames
                            .ZeroDefaultMarginsClearsHeaderFooter);
    });

GEN('#if !defined(OS_CHROMEOS)');
// eslint-disable-next-line no-var
var PrintPreviewLinkContainerTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/link_container.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'print_preview_test_utils.js',
      'link_container_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return link_container_test.suiteName;
  }
};

TEST_F('PrintPreviewLinkContainerTest', 'HideInAppKioskMode', function() {
  this.runMochaTest(link_container_test.TestNames.HideInAppKioskMode);
});

TEST_F('PrintPreviewLinkContainerTest', 'SystemDialogLinkClick', function() {
  this.runMochaTest(link_container_test.TestNames.SystemDialogLinkClick);
});

TEST_F('PrintPreviewLinkContainerTest', 'InvalidState', function() {
  this.runMochaTest(link_container_test.TestNames.InvalidState);
});
GEN('#endif');  // !defined(OS_CHROMEOS)

GEN('#if defined(OS_MACOSX)');
TEST_F('PrintPreviewLinkContainerTest', 'OpenInPreviewLinkClick', function() {
  this.runMochaTest(link_container_test.TestNames.OpenInPreviewLinkClick);
});
GEN('#endif');  // defined(OS_MACOSX)

GEN('#if defined(OS_WIN) || defined(OS_MACOSX)');
// eslint-disable-next-line no-var
var PrintPreviewSystemDialogBrowserTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/app.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      '../test_browser_proxy.js',
      'native_layer_stub.js',
      'plugin_stub.js',
      'print_preview_test_utils.js',
      'system_dialog_browsertest.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return system_dialog_browsertest.suiteName;
  }
};

TEST_F(
    'PrintPreviewSystemDialogBrowserTest', 'LinkTriggersLocalPrint',
    function() {
      this.runMochaTest(
          system_dialog_browsertest.TestNames.LinkTriggersLocalPrint);
    });

TEST_F(
    'PrintPreviewSystemDialogBrowserTest', 'InvalidSettingsDisableLink',
    function() {
      this.runMochaTest(
          system_dialog_browsertest.TestNames.InvalidSettingsDisableLink);
    });
GEN('#endif');  // defined(OS_WIN) || defined(OS_MACOSX)

// eslint-disable-next-line no-var
var PrintPreviewInvalidSettingsBrowserTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/app.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/cr/event_target.js',
      '../settings/test_util.js',
      '../test_browser_proxy.js',
      'cloud_print_interface_stub.js',
      'native_layer_stub.js',
      'plugin_stub.js',
      'print_preview_test_utils.js',
      'invalid_settings_browsertest.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return invalid_settings_browsertest.suiteName;
  }
};

TEST_F(
    'PrintPreviewInvalidSettingsBrowserTest', 'NoPDFPluginError', function() {
      this.runMochaTest(
          invalid_settings_browsertest.TestNames.NoPDFPluginError);
    });

TEST_F(
    'PrintPreviewInvalidSettingsBrowserTest', 'InvalidSettingsError',
    function() {
      this.runMochaTest(
          invalid_settings_browsertest.TestNames.InvalidSettingsError);
    });

TEST_F(
    'PrintPreviewInvalidSettingsBrowserTest', 'InvalidCertificateError',
    function() {
      this.runMochaTest(
          invalid_settings_browsertest.TestNames.InvalidCertificateError);
    });

TEST_F(
    'PrintPreviewInvalidSettingsBrowserTest',
    'InvalidCertificateErrorReselectDestination', function() {
      this.runMochaTest(invalid_settings_browsertest.TestNames
                            .InvalidCertificateErrorReselectDestination);
    });

// eslint-disable-next-line no-var
var PrintPreviewDestinationStoreTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/data/destination_store.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/web_ui_listener_behavior.js',
      '../settings/test_util.js',
      '../test_browser_proxy.js',
      'cloud_print_interface_stub.js',
      'native_layer_stub.js',
      'print_preview_test_utils.js',
      'destination_store_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return destination_store_test.suiteName;
  }

  /** @override */
  get customElementName() {
    // This test is loading a data class, not an element.
    return null;
  }
};

TEST_F(
    'PrintPreviewDestinationStoreTest', 'SingleRecentDestination', function() {
      this.runMochaTest(
          destination_store_test.TestNames.SingleRecentDestination);
    });

TEST_F(
    'PrintPreviewDestinationStoreTest', 'MultipleRecentDestinations',
    function() {
      this.runMochaTest(
          destination_store_test.TestNames.MultipleRecentDestinations);
    });

TEST_F(
    'PrintPreviewDestinationStoreTest', 'MultipleRecentDestinationsOneRequest',
    function() {
      this.runMochaTest(destination_store_test.TestNames
                            .MultipleRecentDestinationsOneRequest);
    });

TEST_F(
    'PrintPreviewDestinationStoreTest', 'DefaultDestinationSelectionRules',
    function() {
      this.runMochaTest(
          destination_store_test.TestNames.DefaultDestinationSelectionRules);
    });

GEN('#if !defined(OS_CHROMEOS)');
TEST_F(
    'PrintPreviewDestinationStoreTest', 'SystemDefaultPrinterPolicy',
    function() {
      this.runMochaTest(
          destination_store_test.TestNames.SystemDefaultPrinterPolicy);
    });
GEN('#endif');

TEST_F(
    'PrintPreviewDestinationStoreTest', 'KioskModeSelectsFirstPrinter',
    function() {
      this.runMochaTest(
          destination_store_test.TestNames.KioskModeSelectsFirstPrinter);
    });

GEN('#if defined(OS_CHROMEOS)');
TEST_F('PrintPreviewDestinationStoreTest', 'NoPrintersShowsError', function() {
  this.runMochaTest(destination_store_test.TestNames.NoPrintersShowsError);
});
GEN('#endif');

TEST_F(
    'PrintPreviewDestinationStoreTest', 'UnreachableRecentCloudPrinter',
    function() {
      this.runMochaTest(
          destination_store_test.TestNames.UnreachableRecentCloudPrinter);
    });

TEST_F('PrintPreviewDestinationStoreTest', 'RecentSaveAsPdf', function() {
  this.runMochaTest(destination_store_test.TestNames.RecentSaveAsPdf);
});

TEST_F(
    'PrintPreviewDestinationStoreTest', 'MultipleRecentDestinationsAccounts',
    function() {
      this.runMochaTest(
          destination_store_test.TestNames.MultipleRecentDestinationsAccounts);
    });

// eslint-disable-next-line no-var
var PrintPreviewDestinationDialogTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/destination_dialog.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/web_ui_listener_behavior.js',
      '//ui/webui/resources/js/cr/event_target.js',
      '../settings/test_util.js',
      '../test_browser_proxy.js',
      'cloud_print_interface_stub.js',
      'native_layer_stub.js',
      'print_preview_test_utils.js',
      'destination_dialog_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return destination_dialog_test.suiteName;
  }
};

TEST_F('PrintPreviewDestinationDialogTest', 'PrinterList', function() {
  this.runMochaTest(destination_dialog_test.TestNames.PrinterList);
});

GEN('#if defined(OS_CHROMEOS)');
TEST_F(
    'PrintPreviewDestinationDialogTest', 'ShowProvisionalDialog', function() {
      this.runMochaTest(
          destination_dialog_test.TestNames.ShowProvisionalDialog);
    });
GEN('#endif');

TEST_F('PrintPreviewDestinationDialogTest', 'UserAccounts', function() {
  this.runMochaTest(destination_dialog_test.TestNames.UserAccounts);
});

// eslint-disable-next-line no-var
var PrintPreviewAdvancedDialogTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/advanced_settings_dialog.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'print_preview_test_utils.js',
      'advanced_dialog_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return advanced_dialog_test.suiteName;
  }
};

TEST_F('PrintPreviewAdvancedDialogTest', 'AdvancedSettings1Option', function() {
  this.runMochaTest(advanced_dialog_test.TestNames.AdvancedSettings1Option);
});

TEST_F(
    'PrintPreviewAdvancedDialogTest', 'AdvancedSettings2Options', function() {
      this.runMochaTest(
          advanced_dialog_test.TestNames.AdvancedSettings2Options);
    });

TEST_F('PrintPreviewAdvancedDialogTest', 'AdvancedSettingsApply', function() {
  this.runMochaTest(advanced_dialog_test.TestNames.AdvancedSettingsApply);
});

TEST_F(
    'PrintPreviewAdvancedDialogTest', 'AdvancedSettingsApplyWithEnter',
    function() {
      this.runMochaTest(
          advanced_dialog_test.TestNames.AdvancedSettingsApplyWithEnter);
    });

TEST_F('PrintPreviewAdvancedDialogTest', 'AdvancedSettingsClose', function() {
  this.runMochaTest(advanced_dialog_test.TestNames.AdvancedSettingsClose);
});

TEST_F('PrintPreviewAdvancedDialogTest', 'AdvancedSettingsFilter', function() {
  this.runMochaTest(advanced_dialog_test.TestNames.AdvancedSettingsFilter);
});

// eslint-disable-next-line no-var
var PrintPreviewPreviewAreaTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/preview_area.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      '../test_browser_proxy.js',
      'native_layer_stub.js',
      'plugin_stub.js',
      'print_preview_test_utils.js',
      'preview_area_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return preview_area_test.suiteName;
  }
};

TEST_F('PrintPreviewPreviewAreaTest', 'StateChanges', function() {
  this.runMochaTest(preview_area_test.TestNames.StateChanges);
});

TEST_F('PrintPreviewPreviewAreaTest', 'ViewportSizeChanges', function() {
  this.runMochaTest(preview_area_test.TestNames.ViewportSizeChanges);
});

// eslint-disable-next-line no-var
var PrintPreviewCustomMarginsTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/margin_control_container.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'print_preview_test_utils.js',
      'custom_margins_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return custom_margins_test.suiteName;
  }
};

TEST_F('PrintPreviewCustomMarginsTest', 'ControlsCheck', function() {
  this.runMochaTest(custom_margins_test.TestNames.ControlsCheck);
});

TEST_F('PrintPreviewCustomMarginsTest', 'SetFromStickySettings', function() {
  this.runMochaTest(custom_margins_test.TestNames.SetFromStickySettings);
});

TEST_F('PrintPreviewCustomMarginsTest', 'DragControls', function() {
  this.runMochaTest(custom_margins_test.TestNames.DragControls);
});

TEST_F('PrintPreviewCustomMarginsTest', 'SetControlsWithTextbox', function() {
  this.runMochaTest(custom_margins_test.TestNames.SetControlsWithTextbox);
});

TEST_F(
    'PrintPreviewCustomMarginsTest', 'RestoreStickyMarginsAfterDefault',
    function() {
      this.runMochaTest(
          custom_margins_test.TestNames.RestoreStickyMarginsAfterDefault);
    });

TEST_F(
    'PrintPreviewCustomMarginsTest', 'MediaSizeClearsCustomMargins',
    function() {
      this.runMochaTest(
          custom_margins_test.TestNames.MediaSizeClearsCustomMargins);
    });

TEST_F(
    'PrintPreviewCustomMarginsTest', 'LayoutClearsCustomMargins', function() {
      this.runMochaTest(
          custom_margins_test.TestNames.LayoutClearsCustomMargins);
    });

TEST_F(
    'PrintPreviewCustomMarginsTest', 'IgnoreDocumentMarginsFromPDF',
    function() {
      this.runMochaTest(
          custom_margins_test.TestNames.IgnoreDocumentMarginsFromPDF);
    });

TEST_F(
    'PrintPreviewCustomMarginsTest', 'MediaSizeClearsCustomMarginsPDF',
    function() {
      this.runMochaTest(
          custom_margins_test.TestNames.MediaSizeClearsCustomMarginsPDF);
    });

TEST_F(
    'PrintPreviewCustomMarginsTest', 'RequestScrollToOutOfBoundsTextbox',
    function() {
      this.runMochaTest(
          custom_margins_test.TestNames.RequestScrollToOutOfBoundsTextbox);
    });

TEST_F('PrintPreviewCustomMarginsTest', 'ControlsDisabledOnError', function() {
  this.runMochaTest(custom_margins_test.TestNames.ControlsDisabledOnError);
});

// eslint-disable-next-line no-var
var PrintPreviewDestinationSearchTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/destination_dialog.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/web_ui_listener_behavior.js',
      '../settings/test_util.js',
      '../test_browser_proxy.js',
      'native_layer_stub.js',
      'print_preview_test_utils.js',
      'destination_search_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return destination_search_test.suiteName;
  }
};

TEST_F(
    'PrintPreviewDestinationSearchTest', 'ReceiveSuccessfulSetup', function() {
      this.runMochaTest(
          destination_search_test.TestNames.ReceiveSuccessfulSetup);
    });

GEN('#if defined(OS_CHROMEOS)');
TEST_F('PrintPreviewDestinationSearchTest', 'ResolutionFails', function() {
  this.runMochaTest(destination_search_test.TestNames.ResolutionFails);
});

TEST_F('PrintPreviewDestinationSearchTest', 'ReceiveFailedSetup', function() {
  this.runMochaTest(destination_search_test.TestNames.ReceiveFailedSetup);
});

TEST_F(
    'PrintPreviewDestinationSearchTest', 'ReceiveSuccessfultSetupWithPolicies',
    function() {
      this.runMochaTest(destination_search_test.TestNames.ResolutionFails);
    });

GEN('#else');  // !defined(OS_CHROMEOS)
TEST_F('PrintPreviewDestinationSearchTest', 'GetCapabilitiesFails', function() {
  this.runMochaTest(destination_search_test.TestNames.GetCapabilitiesFails);
});
GEN('#endif');  // defined(OS_CHROMEOS)

TEST_F('PrintPreviewDestinationSearchTest', 'CloudKioskPrinter', function() {
  this.runMochaTest(destination_search_test.TestNames.CloudKioskPrinter);
});

// eslint-disable-next-line no-var
var PrintPreviewHeaderTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/header.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'header_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return header_test.suiteName;
  }
};

TEST_F('PrintPreviewHeaderTest', 'HeaderPrinterTypes', function() {
  this.runMochaTest(header_test.TestNames.HeaderPrinterTypes);
});

TEST_F('PrintPreviewHeaderTest', 'HeaderWithDuplex', function() {
  this.runMochaTest(header_test.TestNames.HeaderWithDuplex);
});

TEST_F('PrintPreviewHeaderTest', 'HeaderWithCopies', function() {
  this.runMochaTest(header_test.TestNames.HeaderWithCopies);
});

TEST_F('PrintPreviewHeaderTest', 'HeaderChangesForState', function() {
  this.runMochaTest(header_test.TestNames.HeaderChangesForState);
});

TEST_F('PrintPreviewHeaderTest', 'ButtonOrder', function() {
  this.runMochaTest(header_test.TestNames.ButtonOrder);
});

TEST_F('PrintPreviewHeaderTest', 'EnterprisePolicy', function() {
  this.runMochaTest(header_test.TestNames.EnterprisePolicy);
});

// eslint-disable-next-line no-var
var PrintPreviewHeaderNewTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/header_new.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'header_new_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return header_new_test.suiteName;
  }
};

TEST_F('PrintPreviewHeaderNewTest', 'HeaderPrinterTypes', function() {
  this.runMochaTest(header_new_test.TestNames.HeaderPrinterTypes);
});

TEST_F('PrintPreviewHeaderNewTest', 'HeaderWithDuplex', function() {
  this.runMochaTest(header_new_test.TestNames.HeaderWithDuplex);
});

TEST_F('PrintPreviewHeaderNewTest', 'HeaderWithCopies', function() {
  this.runMochaTest(header_new_test.TestNames.HeaderWithCopies);
});

TEST_F('PrintPreviewHeaderNewTest', 'HeaderChangesForState', function() {
  this.runMochaTest(header_new_test.TestNames.HeaderChangesForState);
});

TEST_F('PrintPreviewHeaderNewTest', 'EnterprisePolicy', function() {
  this.runMochaTest(header_new_test.TestNames.EnterprisePolicy);
});

// eslint-disable-next-line no-var
var PrintPreviewButtonStripTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/button_strip.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'button_strip_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return button_strip_test.suiteName;
  }
};

TEST_F('PrintPreviewButtonStripTest', 'ButtonStripChangesForState', function() {
  this.runMochaTest(button_strip_test.TestNames.ButtonStripChangesForState);
});

TEST_F('PrintPreviewButtonStripTest', 'ButtonOrder', function() {
  this.runMochaTest(button_strip_test.TestNames.ButtonOrder);
});

TEST_F('PrintPreviewButtonStripTest', 'ButtonStripFiresEvents', function() {
  this.runMochaTest(button_strip_test.TestNames.ButtonStripFiresEvents);
});

// eslint-disable-next-line no-var
var PrintPreviewDestinationItemTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/destination_list_item.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'print_preview_test_utils.js',
      'destination_item_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return destination_item_test.suiteName;
  }
};

TEST_F('PrintPreviewDestinationItemTest', 'Online', function() {
  this.runMochaTest(destination_item_test.TestNames.Online);
});

TEST_F('PrintPreviewDestinationItemTest', 'Offline', function() {
  this.runMochaTest(destination_item_test.TestNames.Offline);
});

TEST_F('PrintPreviewDestinationItemTest', 'BadCertificate', function() {
  this.runMochaTest(destination_item_test.TestNames.BadCertificate);
});

TEST_F('PrintPreviewDestinationItemTest', 'QueryName', function() {
  this.runMochaTest(destination_item_test.TestNames.QueryName);
});

TEST_F('PrintPreviewDestinationItemTest', 'QueryDescription', function() {
  this.runMochaTest(destination_item_test.TestNames.QueryDescription);
});

// eslint-disable-next-line no-var
var PrintPreviewAdvancedItemTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/advanced_settings_item.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'print_preview_test_utils.js',
      'advanced_item_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return advanced_item_test.suiteName;
  }
};

TEST_F('PrintPreviewAdvancedItemTest', 'DisplaySelect', function() {
  this.runMochaTest(advanced_item_test.TestNames.DisplaySelect);
});

TEST_F('PrintPreviewAdvancedItemTest', 'DisplayInput', function() {
  this.runMochaTest(advanced_item_test.TestNames.DisplayInput);
});

TEST_F('PrintPreviewAdvancedItemTest', 'UpdateSelect', function() {
  this.runMochaTest(advanced_item_test.TestNames.UpdateSelect);
});

TEST_F('PrintPreviewAdvancedItemTest', 'UpdateInput', function() {
  this.runMochaTest(advanced_item_test.TestNames.UpdateInput);
});

TEST_F('PrintPreviewAdvancedItemTest', 'QueryName', function() {
  this.runMochaTest(advanced_item_test.TestNames.QueryName);
});

TEST_F('PrintPreviewAdvancedItemTest', 'QueryOption', function() {
  this.runMochaTest(advanced_item_test.TestNames.QueryOption);
});

// eslint-disable-next-line no-var
var PrintPreviewDestinationListTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/destination_list.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'destination_list_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return destination_list_test.suiteName;
  }
};

TEST_F('PrintPreviewDestinationListTest', 'FilterDestinations', function() {
  this.runMochaTest(destination_list_test.TestNames.FilterDestinations);
});

TEST_F(
    'PrintPreviewDestinationListTest', 'FireDestinationSelected', function() {
      this.runMochaTest(
          destination_list_test.TestNames.FireDestinationSelected);
    });

// eslint-disable-next-line no-var
var PrintPreviewPrintButtonTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/app.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../test_browser_proxy.js',
      'native_layer_stub.js',
      'plugin_stub.js',
      'print_preview_test_utils.js',
      'print_button_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return print_button_test.suiteName;
  }
};

TEST_F('PrintPreviewPrintButtonTest', 'LocalPrintHidePreview', function() {
  this.runMochaTest(print_button_test.TestNames.LocalPrintHidePreview);
});

TEST_F('PrintPreviewPrintButtonTest', 'PDFPrintVisiblePreview', function() {
  this.runMochaTest(print_button_test.TestNames.PDFPrintVisiblePreview);
});

// eslint-disable-next-line no-var
var PrintPreviewKeyEventTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/app.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      '../test_browser_proxy.js',
      'native_layer_stub.js',
      'plugin_stub.js',
      'print_preview_test_utils.js',
      'key_event_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return key_event_test.suiteName;
  }
};

TEST_F('PrintPreviewKeyEventTest', 'EnterTriggersPrint', function() {
  this.runMochaTest(key_event_test.TestNames.EnterTriggersPrint);
});

TEST_F('PrintPreviewKeyEventTest', 'NumpadEnterTriggersPrint', function() {
  this.runMochaTest(key_event_test.TestNames.NumpadEnterTriggersPrint);
});

TEST_F('PrintPreviewKeyEventTest', 'EnterOnInputTriggersPrint', function() {
  this.runMochaTest(key_event_test.TestNames.EnterOnInputTriggersPrint);
});

TEST_F('PrintPreviewKeyEventTest', 'EnterOnDropdownDoesNotPrint', function() {
  this.runMochaTest(key_event_test.TestNames.EnterOnDropdownDoesNotPrint);
});

TEST_F('PrintPreviewKeyEventTest', 'EnterOnButtonDoesNotPrint', function() {
  this.runMochaTest(key_event_test.TestNames.EnterOnButtonDoesNotPrint);
});

TEST_F('PrintPreviewKeyEventTest', 'EnterOnCheckboxDoesNotPrint', function() {
  this.runMochaTest(key_event_test.TestNames.EnterOnCheckboxDoesNotPrint);
});

TEST_F('PrintPreviewKeyEventTest', 'EscapeClosesDialogOnMacOnly', function() {
  this.runMochaTest(key_event_test.TestNames.EscapeClosesDialogOnMacOnly);
});

TEST_F(
    'PrintPreviewKeyEventTest', 'CmdPeriodClosesDialogOnMacOnly', function() {
      this.runMochaTest(
          key_event_test.TestNames.CmdPeriodClosesDialogOnMacOnly);
    });

TEST_F('PrintPreviewKeyEventTest', 'CtrlShiftPOpensSystemDialog', function() {
  this.runMochaTest(key_event_test.TestNames.CtrlShiftPOpensSystemDialog);
});

// eslint-disable-next-line no-var
var PrintPreviewDestinationSettingsTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/destination_settings.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/web_ui_listener_behavior.js',
      '../test_browser_proxy.js',
      '../settings/test_util.js',
      'cloud_print_interface_stub.js',
      'print_preview_test_utils.js',
      'native_layer_stub.js',
      'print_preview_test_utils.js',
      'destination_settings_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return destination_settings_test.suiteName;
  }
};

TEST_F(
    'PrintPreviewDestinationSettingsTest', 'ChangeDropdownState', function() {
      this.runMochaTest(
          destination_settings_test.TestNames.ChangeDropdownState);
    });

TEST_F(
    'PrintPreviewDestinationSettingsTest', 'NoRecentDestinations', function() {
      this.runMochaTest(
          destination_settings_test.TestNames.NoRecentDestinations);
    });

TEST_F('PrintPreviewDestinationSettingsTest', 'RecentDestinations', function() {
  this.runMochaTest(destination_settings_test.TestNames.RecentDestinations);
});

TEST_F('PrintPreviewDestinationSettingsTest', 'SaveAsPdfRecent', function() {
  this.runMochaTest(destination_settings_test.TestNames.SaveAsPdfRecent);
});

TEST_F('PrintPreviewDestinationSettingsTest', 'GoogleDriveRecent', function() {
  this.runMochaTest(destination_settings_test.TestNames.GoogleDriveRecent);
});

TEST_F('PrintPreviewDestinationSettingsTest', 'SelectSaveAsPdf', function() {
  this.runMochaTest(destination_settings_test.TestNames.SelectSaveAsPdf);
});

TEST_F('PrintPreviewDestinationSettingsTest', 'SelectGoogleDrive', function() {
  this.runMochaTest(destination_settings_test.TestNames.SelectGoogleDrive);
});

TEST_F(
    'PrintPreviewDestinationSettingsTest', 'SelectRecentDestination',
    function() {
      this.runMochaTest(
          destination_settings_test.TestNames.SelectRecentDestination);
    });

TEST_F('PrintPreviewDestinationSettingsTest', 'OpenDialog', function() {
  this.runMochaTest(destination_settings_test.TestNames.OpenDialog);
});

TEST_F(
    'PrintPreviewDestinationSettingsTest', 'TwoAccountsRecentDestinations',
    function() {
      this.runMochaTest(
          destination_settings_test.TestNames.TwoAccountsRecentDestinations);
    });

TEST_F(
    'PrintPreviewDestinationSettingsTest', 'UpdateRecentDestinations',
    function() {
      this.runMochaTest(
          destination_settings_test.TestNames.UpdateRecentDestinations);
    });

TEST_F(
    'PrintPreviewDestinationSettingsTest', 'ResetDestinationOnSignOut',
    function() {
      this.runMochaTest(
          destination_settings_test.TestNames.ResetDestinationOnSignOut);
    });

// eslint-disable-next-line no-var
var PrintPreviewScalingSettingsTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/scaling_settings.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'print_preview_test_utils.js',
      'scaling_settings_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return scaling_settings_test.suiteName;
  }
};

TEST_F(
    'PrintPreviewScalingSettingsTest', 'ShowCorrectDropdownOptions',
    function() {
      this.runMochaTest(
          scaling_settings_test.TestNames.ShowCorrectDropdownOptions);
    });

TEST_F('PrintPreviewScalingSettingsTest', 'SetScaling', function() {
  this.runMochaTest(scaling_settings_test.TestNames.SetScaling);
});

TEST_F(
    'PrintPreviewScalingSettingsTest', 'InputNotDisabledOnValidityChange',
    function() {
      this.runMochaTest(
          scaling_settings_test.TestNames.InputNotDisabledOnValidityChange);
    });

// eslint-disable-next-line no-var
var PrintPreviewCopiesSettingsTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/copies_settings.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'print_preview_test_utils.js',
      'copies_settings_test.js',
    ]);
  }
};

TEST_F('PrintPreviewCopiesSettingsTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var PrintPreviewMediaSizeSettingsTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/media_size_settings.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'print_preview_test_utils.js',
      'media_size_settings_test.js',
    ]);
  }
};

TEST_F('PrintPreviewMediaSizeSettingsTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var PrintPreviewDpiSettingsTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/dpi_settings.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'print_preview_test_utils.js',
      'dpi_settings_test.js',
    ]);
  }
};

TEST_F('PrintPreviewDpiSettingsTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var PrintPreviewOtherOptionsSettingsTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/other_options_settings.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'print_preview_test_utils.js',
      'other_options_settings_test.js',
    ]);
  }
};

TEST_F('PrintPreviewOtherOptionsSettingsTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var PrintPreviewLayoutSettingsTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/layout_settings.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'print_preview_test_utils.js',
      'layout_settings_test.js',
    ]);
  }
};

TEST_F('PrintPreviewLayoutSettingsTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var PrintPreviewColorSettingsTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/color_settings.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'print_preview_test_utils.js',
      'color_settings_test.js',
    ]);
  }
};

TEST_F('PrintPreviewColorSettingsTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var PrintPreviewMarginsSettingsTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/margins_settings.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'print_preview_test_utils.js',
      'margins_settings_test.js',
    ]);
  }
};

TEST_F('PrintPreviewMarginsSettingsTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var PrintPreviewPagesPerSheetSettingsTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/pages_per_sheet_settings.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'print_preview_test_utils.js',
      'pages_per_sheet_settings_test.js',
    ]);
  }
};

TEST_F('PrintPreviewPagesPerSheetSettingsTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var PrintPreviewDuplexSettingsTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/duplex_settings.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'print_preview_test_utils.js',
      'duplex_settings_test.js',
    ]);
  }
};

TEST_F('PrintPreviewDuplexSettingsTest', 'All', function() {
  mocha.run();
});

GEN('#if defined(OS_CHROMEOS)');
// eslint-disable-next-line no-var
var PrintPreviewPinSettingsTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/ui/pin_settings.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'print_preview_test_utils.js',
      'pin_settings_test.js',
    ]);
  }
};

TEST_F('PrintPreviewPinSettingsTest', 'All', function() {
  mocha.run();
});
GEN('#endif');

// eslint-disable-next-line no-var
var PrintPreviewUserManagerTest = class extends PrintPreviewTest {
  /** @override */
  get browsePreload() {
    return 'chrome://print/data/user_manager.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      '../test_browser_proxy.js',
      'cloud_print_interface_stub.js',
      'native_layer_stub.js',
      'print_preview_test_utils.js',
      'user_manager_test.js',
    ]);
  }
};

TEST_F('PrintPreviewUserManagerTest', 'All', function() {
  mocha.run();
});
