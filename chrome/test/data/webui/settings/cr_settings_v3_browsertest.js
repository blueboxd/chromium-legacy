// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer 3 elements. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "build/branding_buildflags.h"');
GEN('#include "chrome/browser/ui/ui_features.h"');
GEN('#include "chrome/common/chrome_features.h"');
GEN('#include "components/autofill/core/common/autofill_features.h"');
GEN('#include "components/password_manager/core/common/password_manager_features.h"');
GEN('#include "services/network/public/cpp/features.h"');

/** Test fixture for shared Polymer 3 elements. */
// eslint-disable-next-line no-var
var CrSettingsV3BrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings';
  }

  /** @override */
  get extraLibraries() {
    return [
      '//third_party/mocha/mocha.js',
      '//chrome/test/data/webui/mocha_adapter.js',
    ];
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'network::features::kOutOfBlinkCors',
        'features::kSettingsPolymer3',
      ],
      disabled: [],
    };
  }
};

// eslint-disable-next-line no-var
var CrSettingsAboutPageV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/about_page_tests.m.js';
  }
};

TEST_F('CrSettingsAboutPageV3Test', 'AboutPage', function() {
  mocha.grep('AboutPageTest_AllBuilds').run();
});

GEN('#if BUILDFLAG(GOOGLE_CHROME_BRANDING)');
TEST_F('CrSettingsAboutPageV3Test', 'AboutPage_OfficialBuild', function() {
  mocha.grep('AboutPageTest_OfficialBuilds').run();
});
GEN('#endif');

// eslint-disable-next-line no-var
var CrSettingsLanguagesPageV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/languages_page_tests.m.js';
  }
};

TEST_F('CrSettingsLanguagesPageV3Test', 'AddLanguagesDialog', function() {
  mocha.grep(languages_page_tests.TestNames.AddLanguagesDialog).run();
});

TEST_F('CrSettingsLanguagesPageV3Test', 'LanguageMenu', function() {
  mocha.grep(languages_page_tests.TestNames.LanguageMenu).run();
});

TEST_F('CrSettingsLanguagesPageV3Test', 'Spellcheck', function() {
  mocha.grep(languages_page_tests.TestNames.Spellcheck).run();
});

GEN('#if BUILDFLAG(GOOGLE_CHROME_BRANDING)');
TEST_F('CrSettingsLanguagesPageV3Test', 'SpellcheckOfficialBuild', function() {
  mocha.grep(languages_page_tests.TestNames.SpellcheckOfficialBuild).run();
});
GEN('#endif');

// eslint-disable-next-line no-var
var CrSettingsClearBrowsingDataV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/clear_browsing_data_test.m.js';
  }
};

TEST_F(
    'CrSettingsClearBrowsingDataV3Test', 'ClearBrowsingDataAllPlatforms',
    function() {
      runMochaSuite('ClearBrowsingDataAllPlatforms');
    });

TEST_F('CrSettingsClearBrowsingDataV3Test', 'InstalledApps', () => {
  runMochaSuite('InstalledApps');
});

GEN('#if !defined(OS_CHROMEOS)');
TEST_F(
    'CrSettingsClearBrowsingDataV3Test', 'ClearBrowsingDataDesktop',
    function() {
      runMochaSuite('ClearBrowsingDataDesktop');
    });
GEN('#endif');


// eslint-disable-next-line no-var
var CrSettingsMainPageV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/settings_main_test.m.js';
  }
};

// Copied from Polymer 2 version of tests:
// Times out on Windows Tests (dbg). See https://crbug.com/651296.
// Times out / crashes on chromium.linux/Linux Tests (dbg) crbug.com/667882
GEN('#if !defined(NDEBUG)');
GEN('#define MAYBE_MainPageV3 DISABLED_MainPageV3');
GEN('#else');
GEN('#define MAYBE_MainPageV3 MainPageV3');
GEN('#endif');
TEST_F('CrSettingsMainPageV3Test', 'MAYBE_MainPageV3', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsAutofillSectionCompanyEnabledV3Test =
    class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/autofill_section_test.m.js';
  }

  /** @override */
  get featureList() {
    const list = super.featureList;
    list.enabled.push('autofill::features::kAutofillEnableCompanyName');
    return list;
  }
};

TEST_F('CrSettingsAutofillSectionCompanyEnabledV3Test', 'All', function() {
  // Use 'EnableCompanyName' to inform tests that the feature is enabled.
  const loadTimeDataOverride = {};
  loadTimeDataOverride['EnableCompanyName'] = true;
  loadTimeData.overrideValues(loadTimeDataOverride);
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsAutofillSectionCompanyDisabledV3Test =
    class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/autofill_section_test.m.js';
  }

  /** @override */
  get featureList() {
    const list = super.featureList;
    list.disabled.push('autofill::features::kAutofillEnableCompanyName');
    return list;
  }
};

TEST_F('CrSettingsAutofillSectionCompanyDisabledV3Test', 'All', function() {
  // Use 'EnableCompanyName' to inform tests that the feature is enabled.
  const loadTimeDataOverride = {};
  loadTimeDataOverride['EnableCompanyName'] = false;
  loadTimeData.overrideValues(loadTimeDataOverride);
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsPasswordsSectionV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/passwords_section_test.m.js';
  }
  /** @override */
  get featureList() {
    const list = super.featureList;
    list.enabled.push('password_manager::features::kPasswordCheck');
    return list;
  }
};

TEST_F('CrSettingsPasswordsSectionV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsPasswordsCheckV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/password_check_test.m.js';
  }

  /** @override */
  get featureList() {
    const list = super.featureList;
    list.enabled.push('password_manager::features::kPasswordCheck');
    return list;
  }
};

TEST_F('CrSettingsPasswordsCheckV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsSafetyCheckPageV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/safety_check_page_test.m.js';
  }

  /** @override */
  get featureList() {
    const list = super.featureList;
    list.enabled.push('features::kPrivacySettingsRedesign');
    list.enabled.push('password_manager::features::kPasswordCheck');
    return list;
  }
};

TEST_F('CrSettingsSafetyCheckPageV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsSiteListV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/site_list_tests.m.js';
  }
};

// Copied from Polymer 2 test:
// TODO(crbug.com/929455): flaky, fix.
TEST_F('CrSettingsSiteListV3Test', 'DISABLED_SiteList', function() {
  runMochaSuite('SiteList');
});

TEST_F('CrSettingsSiteListV3Test', 'EditExceptionDialog', function() {
  runMochaSuite('EditExceptionDialog');
});

TEST_F('CrSettingsSiteListV3Test', 'AddExceptionDialog', function() {
  runMochaSuite('AddExceptionDialog');
});

// eslint-disable-next-line no-var
var CrSettingsSiteDetailsV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/site_details_tests.m.js';
  }
};

// Disabling on debug due to flaky timeout on Win7 Tests (dbg)(1) bot.
// https://crbug.com/825304 - later for other platforms in crbug.com/1021219.
// Disabling on Linux CFI due to flaky timeout (crbug.com/1031960).
GEN('#if (!defined(NDEBUG)) || (defined(OS_LINUX) && defined(IS_CFI))');
GEN('#define MAYBE_SiteDetails DISABLED_SiteDetails');
GEN('#else');
GEN('#define MAYBE_SiteDetails SiteDetails');
GEN('#endif');

TEST_F('CrSettingsSiteDetailsV3Test', 'MAYBE_SiteDetails', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsPersonalizationOptionsV3Test =
    class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/personalization_options_test.m.js';
  }
};

TEST_F('CrSettingsPersonalizationOptionsV3Test', 'AllBuilds', function() {
  runMochaSuite('PersonalizationOptionsTests_AllBuilds');
});

GEN('#if BUILDFLAG(GOOGLE_CHROME_BRANDING)');
TEST_F('CrSettingsPersonalizationOptionsV3Test', 'OfficialBuild', function() {
  runMochaSuite('PersonalizationOptionsTests_OfficialBuild');
});
GEN('#endif');

TEST_F('CrSettingsPersonalizationOptionsV3Test', 'AllBuildsOld', function() {
  runMochaSuite('PersonalizationOptionsTests_AllBuilds_Old');
});

// eslint-disable-next-line no-var
var CrSettingsPrivacyPageV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/privacy_page_test.m.js';
  }
};

TEST_F('CrSettingsPrivacyPageV3Test', 'PrivacyPageTests', function() {
  runMochaSuite('PrivacyPage');
});

TEST_F('CrSettingsPrivacyPageV3Test', 'PrivacyPageRedesignTests', function() {
  runMochaSuite('PrivacyPageRedesignEnabled');
});

// TODO(crbug.com/1043665): flaky crash on Linux Tests (dbg).
TEST_F(
    'CrSettingsPrivacyPageV3Test', 'DISABLED_PrivacyPageSoundTests',
    function() {
      runMochaSuite('PrivacyPageSound');
    });

TEST_F('CrSettingsPrivacyPageV3Test', 'UMALoggingTests', function() {
  runMochaSuite('PrivacyPageUMACheck');
});

GEN('#if defined(OS_MACOSX) || defined(OS_WIN)');
// TODO(crbug.com/1043665): disabling due to failures on several builders.
TEST_F(
    'CrSettingsPrivacyPageV3Test', 'DISABLED_CertificateManagerTests',
    function() {
      runMochaSuite('NativeCertificateManager');
    });
GEN('#endif');

// eslint-disable-next-line no-var
var CrSettingsPrivacyPageRedesignV3Test =
    class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/privacy_page_test.m.js';
  }

  get featureList() {
    const list = super.featureList;
    list.enabled.push('features::kPrivacySettingsRedesign');
    return list;
  }
};

TEST_F(
    'CrSettingsPrivacyPageRedesignV3Test', 'HappinessTrackingSurveysTests',
    function() {
      runMochaSuite('HappinessTrackingSurveys');
    });

// eslint-disable-next-line no-var
var CrSettingsRouteV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/route_tests.m.js';
  }
};

TEST_F('CrSettingsRouteV3Test', 'Basic', function() {
  runMochaSuite('route');
});

TEST_F('CrSettingsRouteV3Test', 'DynamicParameters', function() {
  runMochaSuite('DynamicParameters');
});

// Copied from Polymer 2 test:
// Failing on ChromiumOS dbg. https://crbug.com/709442
GEN('#if (defined(OS_WIN) || defined(OS_CHROMEOS)) && !defined(NDEBUG)');
GEN('#define MAYBE_NonExistentRoute DISABLED_NonExistentRoute');
GEN('#else');
GEN('#define MAYBE_NonExistentRoute NonExistentRoute');
GEN('#endif');

TEST_F('CrSettingsRouteV3Test', 'MAYBE_NonExistentRoute', function() {
  runMochaSuite('NonExistentRoute');
});

[['AllSites', 'all_sites_tests.m.js'],
 ['AppearanceFontsPage', 'appearance_fonts_page_test.m.js'],
 ['AppearancePage', 'appearance_page_test.m.js'],
 ['AutofillPage', 'autofill_page_test.m.js'],
 ['BasicPage', 'basic_page_test.m.js'],
 ['CategoryDefaultSetting', 'category_default_setting_tests.m.js'],
 ['CategorySettingExceptions', 'category_setting_exceptions_tests.m.js'],
 ['Checkbox', 'checkbox_tests.m.js'],
 ['ChooserExceptionList', 'chooser_exception_list_tests.m.js'],
 ['ChooserExceptionListEntry', 'chooser_exception_list_entry_tests.m.js'],
 ['CollapseRadioButton', 'collapse_radio_button_tests.m.js'],
 ['ControlledButton', 'controlled_button_tests.m.js'],
 ['ControlledRadioButton', 'controlled_radio_button_tests.m.js'],
 ['CookiesPage', 'cookies_page_test.m.js'],
 ['DoNotTrackToggle', 'do_not_track_toggle_test.m.js'],
 ['DownloadsPage', 'downloads_page_test.m.js'],
 ['DropdownMenu', 'dropdown_menu_tests.m.js'],
 ['ExtensionControlledIndicator', 'extension_controlled_indicator_tests.m.js'],
 ['Languages', 'languages_tests.m.js'],
 ['Menu', 'settings_menu_test.m.js'],
 ['OnStartupPage', 'on_startup_page_tests.m.js'],
 ['PasswordsLeakDetectionToggle', 'passwords_leak_detection_toggle_test.m.js'],
 ['PaymentsSection', 'payments_section_test.m.js'],
 ['PeoplePage', 'people_page_test.m.js'],
 ['PeoplePageSyncControls', 'people_page_sync_controls_test.m.js'],
 ['PeoplePageSyncPage', 'people_page_sync_page_test.m.js'],
 ['Prefs', 'prefs_tests.m.js'],
 ['PrefUtil', 'pref_util_tests.m.js'],
 ['ProtocolHandlers', 'protocol_handlers_tests.m.js'],
 ['RecentSitePermissions', 'recent_site_permissions_test.m.js'],
 ['ResetPage', 'reset_page_test.m.js'],
 ['ResetProfileBanner', 'reset_profile_banner_test.m.js'],
 ['SearchEngines', 'search_engines_page_test.m.js'],
 ['SearchPage', 'search_page_test.m.js'],
 ['Search', 'search_settings_test.m.js'],
 ['SecurityKeysSubpage', 'security_keys_subpage_test.m.js'],
 ['SecurityPage', 'security_page_test.m.js'],
 ['SecureDns', 'secure_dns_test.m.js'],
 // Copied from P2 test: Disabled for flakiness, see https://crbug.com/1061249
 ['SiteData', 'site_data_test.m.js', 'DISABLED_All'],
 ['SiteDataDetails', 'site_data_details_subpage_tests.m.js'],
 ['SiteDetailsPermission', 'site_details_permission_tests.m.js'],
 ['SiteEntry', 'site_entry_tests.m.js'],
 ['SiteFavicon', 'site_favicon_test.m.js'],
 ['SiteListEntry', 'site_list_entry_tests.m.js'],
 ['SiteSettingsPage', 'site_settings_page_test.m.js'],
 ['Slider', 'settings_slider_tests.m.js'],
 ['StartupUrlsPage', 'startup_urls_page_test.m.js'],
 ['Subpage', 'settings_subpage_test.m.js'],
 ['SyncAccountControl', 'sync_account_control_test.m.js'],
 ['Textarea', 'settings_textarea_tests.m.js'],
 ['ToggleButton', 'settings_toggle_button_tests.m.js'],
 ['ZoomLevels', 'zoom_levels_tests.m.js'],
].forEach(test => registerTest(...test));

GEN('#if defined(OS_CHROMEOS)');
[['PasswordsSectionCros', 'passwords_section_test_cros.m.js'],
 ['PeoplePageChromeOS', 'people_page_test_cros.m.js'],
 // Copied from Polymer 2 test. TODO(crbug.com/929455): flaky, fix.
 ['SiteListChromeOS', 'site_list_tests_cros.m.js', 'DISABLED_AndroidSmsInfo'],
].forEach(test => registerTest(...test));
GEN('#endif  // defined(OS_CHROMEOS)');

GEN('#if !defined(OS_MACOSX)');
[['EditDictionaryPage', 'edit_dictionary_page_test.m.js'],
].forEach(test => registerTest(...test));
GEN('#endif  //!defined(OS_MACOSX)');

GEN('#if !defined(OS_CHROMEOS)');
[['DefaultBrowser', 'default_browser_browsertest.m.js'],
 ['ImportDataDialog', 'import_data_dialog_test.m.js'],
 ['PeoplePageManageProfile', 'people_page_manage_profile_test.m.js'],
 ['SystemPage', 'system_page_tests.m.js'],
].forEach(test => registerTest(...test));
GEN('#endif  // !defined(OS_CHROMEOS)');

GEN('#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)');
[['ChromeCleanupPage', 'chrome_cleanup_page_test.m.js'],
 ['IncompatibleApplicationsPage', 'incompatible_applications_page_test.m.js'],
].forEach(test => registerTest(...test));
GEN('#endif  // defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)');

GEN('#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !defined(OS_CHROMEOS)');
registerTest('MetricsReporting', 'metrics_reporting_tests.m.js');
GEN('#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING) && !defined(OS_CHROMEOS)');

function registerTest(testName, module, caseName) {
  const className = `CrSettings${testName}V3Test`;
  this[className] = class extends CrSettingsV3BrowserTest {
    /** @override */
    get browsePreload() {
      return `chrome://settings/test_loader.html?module=settings/${module}`;
    }
  };

  TEST_F(className, caseName || 'All', () => mocha.run());
}
