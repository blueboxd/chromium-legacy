// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Settings tests. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);
GEN('#include "chrome/browser/ui/webui/extensions/' +
    'extension_settings_browsertest.h"');

/**
 * Basic test fixture for the MD chrome://extensions page. Installs no
 * extensions.
 */
const CrExtensionsBrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/';
  }

  /** @override */
  get extraLibraries() {
    return [
      ...super.extraLibraries,
      '//ui/webui/resources/js/assert.js',
      'test_util.js',
      '../mock_controller.js',
      '../../../../../ui/webui/resources/js/promise_resolver.js',
      '../../../../../ui/webui/resources/js/webui_resource_test.js',
      '../fake_chrome_event.js',
      '../settings/test_util.js',
      '../test_browser_proxy.js',
      'test_service.js',
    ];
  }

  /** @override */
  get typedefCppFixture() {
    return 'ExtensionSettingsUIBrowserTest';
  }

  // The name of the mocha suite. Should be overriden by subclasses.
  get suiteName() {
    return null;
  }

  /** @override */
  get loaderFile() {
    return 'subpage_loader.html';
  }

  // The name of the custom element under test. Should be overridden by
  // subclasses that are loading the URL of a non-element.
  get customElementName() {
    const r = /chrome\:\/\/extensions\/(([a-zA-Z-_]+)\/)?([a-zA-Z-_]+)\.html/;
    const result = r.exec(this.browsePreload);
    if (result && result.length > 3) {
      const element = result[3].replace(/_/gi, '-');
      return result[2] === undefined ? 'extensions-' + element : element;
    }

    // Loading the main page, return extensions manager.
    return 'extensions-manager';
  }

  /** @param {string} testName The name of the test to run. */
  runMochaTest(testName) {
    runMochaTest(this.suiteName, testName);
  }
};

/**
 * Test fixture with one installed extension.
 */
const CrExtensionsBrowserTestWithInstalledExtension =
    class extends CrExtensionsBrowserTest {
  /** @override */
  testGenPreamble() {
    GEN('  InstallGoodExtension();');
    GEN('  SetAutoConfirmUninstall();');
  }
};

////////////////////////////////////////////////////////////////////////////////
// Extension Sidebar Tests

// eslint-disable-next-line no-var
var CrExtensionsSidebarTest = class extends CrExtensionsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/sidebar.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'sidebar_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return extension_sidebar_tests.suiteName;
  }
};

TEST_F('CrExtensionsSidebarTest', 'LayoutAndClickHandlers', function() {
  this.runMochaTest(extension_sidebar_tests.TestNames.LayoutAndClickHandlers);
});

TEST_F('CrExtensionsSidebarTest', 'SetSelected', function() {
  this.runMochaTest(extension_sidebar_tests.TestNames.SetSelected);
});

////////////////////////////////////////////////////////////////////////////////
// Extension Toolbar Tests

// eslint-disable-next-line no-var
var CrExtensionsToolbarTest = class extends CrExtensionsBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'toolbar_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return extension_toolbar_tests.suiteName;
  }
};

TEST_F('CrExtensionsToolbarTest', 'Layout', function() {
  this.runMochaTest(extension_toolbar_tests.TestNames.Layout);
});

TEST_F('CrExtensionsToolbarTest', 'DevModeToggle', function() {
  this.runMochaTest(extension_toolbar_tests.TestNames.DevModeToggle);
});

// TODO(crbug.com/882342) Disabled on other platforms but MacOS due to timeouts.
GEN('#if !defined(OS_MACOSX)');
GEN('#define MAYBE_ClickHandlers DISABLED_ClickHandlers');
GEN('#else');
GEN('#define MAYBE_ClickHandlers ClickHandlers');
GEN('#endif');
TEST_F('CrExtensionsToolbarTest', 'MAYBE_ClickHandlers', function() {
  this.runMochaTest(extension_toolbar_tests.TestNames.ClickHandlers);
});

GEN('#if defined(OS_CHROMEOS)');
TEST_F('CrExtensionsToolbarTest', 'KioskMode', function() {
  this.runMochaTest(extension_toolbar_tests.TestNames.KioskMode);
});
GEN('#endif');

////////////////////////////////////////////////////////////////////////////////
// Extension Item Tests

// eslint-disable-next-line no-var
var CrExtensionsItemsTest = class extends CrExtensionsBrowserTest {
  get browsePreload() {
    return 'chrome://extensions/item.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'item_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return extension_item_tests.suiteName;
  }
};

TEST_F('CrExtensionsItemsTest', 'NormalState', function() {
  this.runMochaTest(
      extension_item_tests.TestNames.ElementVisibilityNormalState);
});

TEST_F('CrExtensionsItemsTest', 'DeveloperState', function() {
  this.runMochaTest(
      extension_item_tests.TestNames.ElementVisibilityDeveloperState);
});

TEST_F('CrExtensionsItemsTest', 'ClickableItems', function() {
  this.runMochaTest(extension_item_tests.TestNames.ClickableItems);
});

TEST_F('CrExtensionsItemsTest', 'FailedReloadFiresLoadError', function() {
  this.runMochaTest(extension_item_tests.TestNames.FailedReloadFiresLoadError);
});

TEST_F('CrExtensionsItemsTest', 'Warnings', function() {
  this.runMochaTest(extension_item_tests.TestNames.Warnings);
});

TEST_F('CrExtensionsItemsTest', 'SourceIndicator', function() {
  this.runMochaTest(extension_item_tests.TestNames.SourceIndicator);
});

TEST_F('CrExtensionsItemsTest', 'EnableToggle', function() {
  this.runMochaTest(extension_item_tests.TestNames.EnableToggle);
});

TEST_F('CrExtensionsItemsTest', 'RemoveButton', function() {
  this.runMochaTest(extension_item_tests.TestNames.RemoveButton);
});

TEST_F('CrExtensionsItemsTest', 'HtmlInName', function() {
  this.runMochaTest(extension_item_tests.TestNames.HtmlInName);
});

////////////////////////////////////////////////////////////////////////////////
// Extension Activity Log Tests

// eslint-disable-next-line no-var
var CrExtensionsActivityLogTest = class extends CrExtensionsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/activity_log/activity_log.html';
  }

  get extraLibraries() {
    return super.extraLibraries.concat([
      'activity_log_test.js',
    ]);
  }

  /** @override */
  get customElementName() {
    // This element's naming scheme is unusual.
    return 'extensions-activity-log';
  }
};

TEST_F('CrExtensionsActivityLogTest', 'All', () => {
  mocha.run();
});

////////////////////////////////////////////////////////////////////////////////
// Extension Activity Log History Tests

// eslint-disable-next-line no-var
var CrExtensionsActivityLogHistoryTest = class extends CrExtensionsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/activity_log/activity_log_history.html';
  }

  get extraLibraries() {
    return super.extraLibraries.concat([
      'activity_log_history_test.js',
    ]);
  }
};

TEST_F('CrExtensionsActivityLogHistoryTest', 'All', () => {
  mocha.run();
});

////////////////////////////////////////////////////////////////////////////////
// Extension Activity Log Item Tests

// eslint-disable-next-line no-var
var CrExtensionsActivityLogHistoryItemTest =
    class extends CrExtensionsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/activity_log/activity_log_history_item.html';
  }

  get extraLibraries() {
    return super.extraLibraries.concat([
      'activity_log_history_item_test.js',
    ]);
  }
};

TEST_F('CrExtensionsActivityLogHistoryItemTest', 'All', () => {
  mocha.run();
});

////////////////////////////////////////////////////////////////////////////////
// Extension Activity Log Stream Tests

// eslint-disable-next-line no-var
var CrExtensionsActivityLogStreamTest = class extends CrExtensionsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/activity_log/activity_log_stream.html';
  }

  get extraLibraries() {
    return super.extraLibraries.concat([
      'activity_log_stream_test.js',
    ]);
  }
};

TEST_F('CrExtensionsActivityLogStreamTest', 'All', () => {
  mocha.run();
});

////////////////////////////////////////////////////////////////////////////////
// Extension Activity Log Stream Item Tests

// eslint-disable-next-line no-var
var CrExtensionsActivityLogStreamItemTest =
    class extends CrExtensionsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/activity_log/activity_log_stream_item.html';
  }

  get extraLibraries() {
    return super.extraLibraries.concat([
      'activity_log_stream_item_test.js',
    ]);
  }
};

TEST_F('CrExtensionsActivityLogStreamItemTest', 'All', () => {
  mocha.run();
});

////////////////////////////////////////////////////////////////////////////////
// Extension Detail View Tests

// eslint-disable-next-line no-var
var CrExtensionsDetailViewTest = class extends CrExtensionsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/detail_view.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'detail_view_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return extension_detail_view_tests.suiteName;
  }
};

TEST_F('CrExtensionsDetailViewTest', 'Layout', function() {
  this.runMochaTest(extension_detail_view_tests.TestNames.Layout);
});

TEST_F('CrExtensionsDetailViewTest', 'LayoutSource', function() {
  this.runMochaTest(extension_detail_view_tests.TestNames.LayoutSource);
});

TEST_F('CrExtensionsDetailViewTest', 'ClickableElements', function() {
  this.runMochaTest(extension_detail_view_tests.TestNames.ClickableElements);
});

TEST_F('CrExtensionsDetailViewTest', 'IndicatorTest', function() {
  this.runMochaTest(extension_detail_view_tests.TestNames.Indicator);
});

TEST_F('CrExtensionsDetailViewTest', 'Warnings', function() {
  this.runMochaTest(extension_detail_view_tests.TestNames.Warnings);
});

////////////////////////////////////////////////////////////////////////////////
// Extension Item List Tests

// eslint-disable-next-line no-var
var CrExtensionsItemListTest = class extends CrExtensionsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/item_list.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'item_list_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return extension_item_list_tests.suiteName;
  }
};

// This test is flaky on Mac10.9 Tests (dbg). See https://crbug.com/771099.
GEN('#if defined(OS_MACOSX)');
GEN('#define MAYBE_Filtering DISABLED_Filtering');
GEN('#else');
GEN('#define MAYBE_Filtering Filtering');
GEN('#endif');
TEST_F('CrExtensionsItemListTest', 'MAYBE_Filtering', function() {
  this.runMochaTest(extension_item_list_tests.TestNames.Filtering);
});

// This test is flaky on Mac10.9 Tests (dbg). See https://crbug.com/771099.
GEN('#if defined(OS_MACOSX)');
GEN('#define MAYBE_NoItems DISABLED_NoItems');
GEN('#else');
GEN('#define MAYBE_NoItems NoItems');
GEN('#endif');
TEST_F('CrExtensionsItemListTest', 'MAYBE_NoItems', function() {
  this.runMochaTest(extension_item_list_tests.TestNames.NoItemsMsg);
});

TEST_F('CrExtensionsItemListTest', 'NoSearchResults', function() {
  this.runMochaTest(extension_item_list_tests.TestNames.NoSearchResultsMsg);
});

TEST_F('CrExtensionsItemListTest', 'LoadTimeData', function() {
  this.runMochaTest(extension_item_list_tests.TestNames.LoadTimeData);
});

////////////////////////////////////////////////////////////////////////////////
// Extension Load Error Tests

// eslint-disable-next-line no-var
var CrExtensionsLoadErrorTest = class extends CrExtensionsBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'load_error_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return extension_load_error_tests.suiteName;
  }
};

TEST_F('CrExtensionsLoadErrorTest', 'RetryError', function() {
  this.runMochaTest(extension_load_error_tests.TestNames.RetryError);
});

TEST_F('CrExtensionsLoadErrorTest', 'RetrySuccess', function() {
  this.runMochaTest(extension_load_error_tests.TestNames.RetrySuccess);
});

TEST_F('CrExtensionsLoadErrorTest', 'CodeSection', function() {
  this.runMochaTest(extension_load_error_tests.TestNames.CodeSection);
});

////////////////////////////////////////////////////////////////////////////////
// Extension Manager Tests

// eslint-disable-next-line no-var
var CrExtensionsManagerUnitTest = class extends CrExtensionsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/manager.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'test_kiosk_browser_proxy.js',
      'manager_unit_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return extension_manager_tests.suiteName;
  }
};

TEST_F('CrExtensionsManagerUnitTest', 'ItemOrder', function() {
  this.runMochaTest(extension_manager_tests.TestNames.ItemOrder);
});

TEST_F('CrExtensionsManagerUnitTest', 'SetItemData', function() {
  this.runMochaTest(extension_manager_tests.TestNames.SetItemData);
});

TEST_F('CrExtensionsManagerUnitTest', 'UpdateItemData', function() {
  this.runMochaTest(extension_manager_tests.TestNames.UpdateItemData);
});

TEST_F('CrExtensionsManagerUnitTest', 'ProfileSettings', function() {
  this.runMochaTest(extension_manager_tests.TestNames.ProfileSettings);
});

TEST_F('CrExtensionsManagerUnitTest', 'Uninstall', function() {
  this.runMochaTest(extension_manager_tests.TestNames.Uninstall);
});

// Flaky since r621915: https://crbug.com/922490
TEST_F(
    'CrExtensionsManagerUnitTest', 'DISABLED_UninstallFromDetails', function() {
      this.runMochaTest(extension_manager_tests.TestNames.UninstallFromDetails);
    });

TEST_F('CrExtensionsManagerUnitTest', 'ToggleIncognito', function() {
  this.runMochaTest(extension_manager_tests.TestNames.ToggleIncognitoMode);
});

TEST_F('CrExtensionsManagerUnitTest', 'EnableAndDisable', function() {
  this.runMochaTest(extension_manager_tests.TestNames.EnableAndDisable);
});

GEN('#if defined(OS_CHROMEOS)');
TEST_F('CrExtensionsManagerUnitTest', 'KioskMode', function() {
  this.runMochaTest(extension_manager_tests.TestNames.KioskMode);
});
GEN('#endif');

// eslint-disable-next-line no-var
var CrExtensionsManagerUnitTestWithActivityLogFlag =
    class extends CrExtensionsManagerUnitTest {
  /** @override */
  get commandLineSwitches() {
    return [{
      switchName: 'enable-extension-activity-logging',
    }];
  }
};

TEST_F(
    'CrExtensionsManagerUnitTestWithActivityLogFlag', 'UpdateFromActivityLog',
    function() {
      this.runMochaTest(
          extension_manager_tests.TestNames.UpdateFromActivityLog);
    });

// eslint-disable-next-line no-var
var CrExtensionsManagerTestWithMultipleExtensionTypesInstalled =
    class extends CrExtensionsBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'manager_test.js',
    ]);
  }

  /** @override */
  testGenPreamble() {
    GEN('  InstallGoodExtension();');
    GEN('  InstallPackagedApp();');
    GEN('  InstallHostedApp();');
    GEN('  InstallPlatformApp();');
  }

  /** @override */
  get suiteName() {
    return extension_manager_tests.suiteName;
  }
};

TEST_F(
    'CrExtensionsManagerTestWithMultipleExtensionTypesInstalled',
    'ItemListVisibility', function() {
      this.runMochaTest(extension_manager_tests.TestNames.ItemListVisibility);
    });

TEST_F(
    'CrExtensionsManagerTestWithMultipleExtensionTypesInstalled', 'SplitItems',
    function() {
      this.runMochaTest(extension_manager_tests.TestNames.SplitItems);
    });

TEST_F(
    'CrExtensionsManagerTestWithMultipleExtensionTypesInstalled', 'ChangePages',
    function() {
      this.runMochaTest(extension_manager_tests.TestNames.ChangePages);
    });

TEST_F(
    'CrExtensionsManagerTestWithMultipleExtensionTypesInstalled',
    'PageTitleUpdate', function() {
      this.runMochaTest(extension_manager_tests.TestNames.PageTitleUpdate);
    });

// eslint-disable-next-line no-var
var CrExtensionsManagerTestWithIdQueryParam =
    class extends CrExtensionsBrowserTestWithInstalledExtension {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/?id=ldnnhddmnhbkjipkidpdiheffobcpfmf';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'manager_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return extension_manager_tests.suiteName;
  }
};

TEST_F(
    'CrExtensionsManagerTestWithIdQueryParam', 'NavigationToDetails',
    function() {
      this.runMochaTest(
          extension_manager_tests.TestNames.UrlNavigationToDetails);
    });

TEST_F(
    'CrExtensionsManagerTestWithIdQueryParam', 'UrlNavigationToActivityLogFail',
    function() {
      this.runMochaTest(
          extension_manager_tests.TestNames.UrlNavigationToActivityLogFail);
    });

CrExtensionsManagerTestWithActivityLogFlag =
    class extends CrExtensionsManagerTestWithIdQueryParam {
  /** @override */
  get commandLineSwitches() {
    return [{
      switchName: 'enable-extension-activity-logging',
    }];
  }
};

TEST_F(
    'CrExtensionsManagerTestWithActivityLogFlag',
    'UrlNavigationToActivityLogSuccess', function() {
      this.runMochaTest(
          extension_manager_tests.TestNames.UrlNavigationToActivityLogSuccess);
    });

////////////////////////////////////////////////////////////////////////////////
// Extension Keyboard Shortcuts Tests

// eslint-disable-next-line no-var
var CrExtensionsShortcutTest = class extends CrExtensionsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/keyboard_shortcuts.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'keyboard_shortcuts_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return extension_shortcut_tests.suiteName;
  }
};

TEST_F('CrExtensionsShortcutTest', 'Layout', function() {
  this.runMochaTest(extension_shortcut_tests.TestNames.Layout);
});

TEST_F('CrExtensionsShortcutTest', 'IsValidKeyCode', function() {
  this.runMochaTest(extension_shortcut_tests.TestNames.IsValidKeyCode);
});

TEST_F('CrExtensionsShortcutTest', 'KeyStrokeToString', function() {
  this.runMochaTest(extension_shortcut_tests.TestNames.IsValidKeyCode);
});

TEST_F('CrExtensionsShortcutTest', 'ScopeChange', function() {
  this.runMochaTest(extension_shortcut_tests.TestNames.ScopeChange);
});

// eslint-disable-next-line no-var
var CrExtensionsShortcutInputTest = class extends CrExtensionsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/keyboard_shortcuts.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'shortcut_input_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return extension_shortcut_input_tests.suiteName;
  }
};

TEST_F('CrExtensionsShortcutInputTest', 'Basic', function() {
  this.runMochaTest(extension_shortcut_input_tests.TestNames.Basic);
});

////////////////////////////////////////////////////////////////////////////////
// Extension Pack Dialog Tests

// eslint-disable-next-line no-var
var CrExtensionsPackDialogTest = class extends CrExtensionsBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'pack_dialog_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return extension_pack_dialog_tests.suiteName;
  }
};

TEST_F('CrExtensionsPackDialogTest', 'Interaction', function() {
  this.runMochaTest(extension_pack_dialog_tests.TestNames.Interaction);
});

// Disabling on Windows due to flaky timeout on some build bots.
// http://crbug.com/832885
GEN('#if defined(OS_WIN)');
GEN('#define MAYBE_PackSuccess DISABLED_PackSuccess');
GEN('#else');
GEN('#define MAYBE_PackSuccess PackSuccess');
GEN('#endif');
TEST_F('CrExtensionsPackDialogTest', 'MAYBE_PackSuccess', function() {
  this.runMochaTest(extension_pack_dialog_tests.TestNames.PackSuccess);
});

TEST_F('CrExtensionsPackDialogTest', 'PackError', function() {
  this.runMochaTest(extension_pack_dialog_tests.TestNames.PackError);
});

// Temporarily disabling on Mac due to flakiness.
// http://crbug.com/877109
GEN('#if defined(OS_MACOSX)');
GEN('#define MAYBE_PackWarning DISABLED_PackWarning');
GEN('#else');
GEN('#define MAYBE_PackWarning PackWarning');
GEN('#endif');
TEST_F('CrExtensionsPackDialogTest', 'MAYBE_PackWarning', function() {
  this.runMochaTest(extension_pack_dialog_tests.TestNames.PackWarning);
});

////////////////////////////////////////////////////////////////////////////////
// Extension Options Dialog Tests

// eslint-disable-next-line no-var
var CrExtensionsOptionsDialogTest = class extends CrExtensionsBrowserTest {
  /** @override */
  testGenPreamble() {
    GEN('  InstallExtensionWithInPageOptions();');
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'options_dialog_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return extension_options_dialog_tests.suiteName;
  }
};

TEST_F('CrExtensionsOptionsDialogTest', 'Layout', function() {
  this.runMochaTest(extension_options_dialog_tests.TestNames.Layout);
});

////////////////////////////////////////////////////////////////////////////////
// Extension Error Page Tests

// eslint-disable-next-line no-var
var CrExtensionsErrorPageTest = class extends CrExtensionsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/error_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'error_page_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return extension_error_page_tests.suiteName;
  }
};

TEST_F('CrExtensionsErrorPageTest', 'Layout', function() {
  this.runMochaTest(extension_error_page_tests.TestNames.Layout);
});

TEST_F('CrExtensionsErrorPageTest', 'CodeSection', function() {
  this.runMochaTest(extension_error_page_tests.TestNames.CodeSection);
});

TEST_F('CrExtensionsErrorPageTest', 'ErrorSelection', function() {
  this.runMochaTest(extension_error_page_tests.TestNames.ErrorSelection);
});

////////////////////////////////////////////////////////////////////////////////
// Extension Code Section Tests

// eslint-disable-next-line no-var
var CrExtensionsCodeSectionTest = class extends CrExtensionsBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'code_section_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return extension_code_section_tests.suiteName;
  }
};

TEST_F('CrExtensionsCodeSectionTest', 'Layout', function() {
  this.runMochaTest(extension_code_section_tests.TestNames.Layout);
});

TEST_F('CrExtensionsCodeSectionTest', 'LongSource', function() {
  this.runMochaTest(extension_code_section_tests.TestNames.LongSource);
});

////////////////////////////////////////////////////////////////////////////////
// Extension Navigation Helper Tests

// eslint-disable-next-line no-var
var CrExtensionsNavigationHelperTest = class extends CrExtensionsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/navigation_helper.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'navigation_helper_test.js',
    ]);
  }

  /** @override */
  get suiteName() {
    return extension_navigation_helper_tests.suiteName;
  }

  /** @override */
  get customElementName() {
    // This test is verifying a class, not a custom element.
    return null;
  }
};

TEST_F('CrExtensionsNavigationHelperTest', 'Basic', function() {
  this.runMochaTest(extension_navigation_helper_tests.TestNames.Basic);
});

TEST_F('CrExtensionsNavigationHelperTest', 'Conversion', function() {
  this.runMochaTest(extension_navigation_helper_tests.TestNames.Conversions);
});

TEST_F('CrExtensionsNavigationHelperTest', 'PushAndReplaceState', function() {
  this.runMochaTest(
      extension_navigation_helper_tests.TestNames.PushAndReplaceState);
});

TEST_F('CrExtensionsNavigationHelperTest', 'SupportedRoutes', function() {
  this.runMochaTest(
      extension_navigation_helper_tests.TestNames.SupportedRoutes);
});

////////////////////////////////////////////////////////////////////////////////
// Error Console tests

// eslint-disable-next-line no-var
var CrExtensionsErrorConsoleTest = class extends CrExtensionsBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'error_console_test.js',
    ]);
  }

  /** @override */
  get browsePreload() {
    return 'chrome://extensions/?errors=oehidglfoeondlkoeloailjdmmghacge';
  }

  /** @override */
  testGenPreamble() {
    GEN('  SetDevModeEnabled(true);');
    GEN('  EnableErrorConsole();');
    GEN('  InstallErrorsExtension();');
  }

  /** @override */
  testGenPostamble() {
    GEN('  SetDevModeEnabled(false);');  // Return this to default.
  }
};

TEST_F('CrExtensionsErrorConsoleTest', 'TestUpDownErrors', () => {
  mocha.run();
});

////////////////////////////////////////////////////////////////////////////////
// extensions-toggle-row tests.

// eslint-disable-next-line no-var
var CrExtensionsToggleRowTest = class extends CrExtensionsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/toggle_row.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'toggle_row_test.js',
    ]);
  }
};

TEST_F('CrExtensionsToggleRowTest', 'ToggleRowTest', function() {
  mocha.run();
});

////////////////////////////////////////////////////////////////////////////////
// kiosk mode tests.

GEN('#if defined(OS_CHROMEOS)');

// eslint-disable-next-line no-var
var CrExtensionsKioskModeTest = class extends CrExtensionsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/kiosk_dialog.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'test_kiosk_browser_proxy.js',
      'kiosk_mode_test.js',
    ]);
  }
  /** @override */
  get suiteName() {
    return extension_kiosk_mode_tests.suiteName;
  }
};

TEST_F('CrExtensionsKioskModeTest', 'AddButton', function() {
  this.runMochaTest(extension_kiosk_mode_tests.TestNames.AddButton);
});

TEST_F('CrExtensionsKioskModeTest', 'Layout', function() {
  this.runMochaTest(extension_kiosk_mode_tests.TestNames.Layout);
});

TEST_F('CrExtensionsKioskModeTest', 'AutoLaunch', function() {
  this.runMochaTest(extension_kiosk_mode_tests.TestNames.AutoLaunch);
});

TEST_F('CrExtensionsKioskModeTest', 'Bailout', function() {
  this.runMochaTest(extension_kiosk_mode_tests.TestNames.Bailout);
});

TEST_F('CrExtensionsKioskModeTest', 'Updated', function() {
  this.runMochaTest(extension_kiosk_mode_tests.TestNames.Updated);
});

TEST_F('CrExtensionsKioskModeTest', 'AddError', function() {
  this.runMochaTest(extension_kiosk_mode_tests.TestNames.AddError);
});

GEN('#endif');

////////////////////////////////////////////////////////////////////////////////
// RuntimeHostsDialog tests

// eslint-disable-next-line no-var
var CrExtensionsRuntimeHostsDialogTest = class extends CrExtensionsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/runtime_hosts_dialog.html';
  }

  get extraLibraries() {
    return super.extraLibraries.concat([
      'runtime_hosts_dialog_test.js',
    ]);
  }
};

TEST_F('CrExtensionsRuntimeHostsDialogTest', 'All', () => {
  mocha.run();
});

////////////////////////////////////////////////////////////////////////////////
// RuntimeHostPermissions tests

// eslint-disable-next-line no-var
var CrExtensionsRuntimeHostPermissionsTest =
    class extends CrExtensionsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/runtime_host_permissions.html';
  }

  get extraLibraries() {
    return super.extraLibraries.concat([
      'runtime_host_permissions_test.js',
    ]);
  }
};

TEST_F('CrExtensionsRuntimeHostPermissionsTest', 'All', () => {
  mocha.run();
});

////////////////////////////////////////////////////////////////////////////////
// HostPermissionsToggleList tests

// eslint-disable-next-line no-var
var CrExtensionsHostPermissionsToggleListTest =
    class extends CrExtensionsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/host_permissions_toggle_list.html';
  }

  get extraLibraries() {
    return super.extraLibraries.concat([
      'host_permissions_toggle_list_test.js',
    ]);
  }
};

TEST_F('CrExtensionsHostPermissionsToggleListTest', 'All', () => {
  mocha.run();
});
