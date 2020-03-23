// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer 3 elements. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);
GEN('#include "services/network/public/cpp/features.h"');

GEN('#include "build/branding_buildflags.h"');
GEN('#include "components/autofill/core/common/autofill_features.h"');
GEN('#include "components/password_manager/core/common/password_manager_features.h"');

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
    return {enabled: ['network::features::kOutOfBlinkCors'], disabled: []};
  }
};

// eslint-disable-next-line no-var
var CrControlledButtonV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/controlled_button_tests.m.js';
  }
};

TEST_F('CrControlledButtonV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrControlledRadioButtonV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/controlled_radio_button_tests.m.js';
  }
};

TEST_F('CrControlledRadioButtonV3Test', 'All', function() {
  mocha.run();
});

GEN('#if !defined(OS_CHROMEOS)');

// eslint-disable-next-line no-var
var CrSettingsDefaultBrowserV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/default_browser_browsertest.m.js';
  }
};

TEST_F('CrSettingsDefaultBrowserV3Test', 'All', function() {
  mocha.run();
});

GEN('#endif  // !defined(OS_CHROMEOS)');

// eslint-disable-next-line no-var
var CrSettingsDownloadsPageV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/downloads_page_test.m.js';
  }
};

TEST_F('CrSettingsDownloadsPageV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsCheckboxV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/checkbox_tests.m.js';
  }
};

TEST_F('CrSettingsCheckboxV3Test', 'All', function() {
  mocha.run();
});

GEN('#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)');

// eslint-disable-next-line no-var
var CrSettingsChromeCleanupPageV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/chrome_cleanup_page_test.m.js';
  }
};

TEST_F('CrSettingsChromeCleanupPageV3Test', 'All', function() {
  mocha.run();
});

GEN('#endif  // defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)');

// eslint-disable-next-line no-var
var CrSettingsDropdownMenuV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/dropdown_menu_tests.m.js';
  }
};

TEST_F('CrSettingsDropdownMenuV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsExtensionControlledIndicatorV3Test =
    class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/extension_controlled_indicator_tests.m.js';
  }
};

TEST_F('CrSettingsExtensionControlledIndicatorV3Test', 'All', function() {
  mocha.run();
});

GEN('#if !defined(OS_CHROMEOS)');

// eslint-disable-next-line no-var
var CrSettingsImportDataDialogV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/import_data_dialog_test.m.js';
  }
};

TEST_F('CrSettingsImportDataDialogV3Test', 'All', function() {
  mocha.run();
});

GEN('#endif  // !defined(OS_CHROMEOS)');

GEN('#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)');

// eslint-disable-next-line no-var
var CrSettingsIncompatibleApplicationsPageV3Test =
    class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/incompatible_applications_page_test.m.js';
  }
};

TEST_F('CrSettingsIncompatibleApplicationsPageV3Test', 'All', function() {
  mocha.run();
});

GEN('#endif  // defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)');

GEN('#if !defined(OS_CHROMEOS)');

// eslint-disable-next-line no-var
var CrSettingsPeoplePageManageProfileV3Test =
    class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/people_page_manage_profile_test.m.js';
  }
};

TEST_F('CrSettingsPeoplePageManageProfileV3Test', 'All', function() {
  mocha.run();
});

GEN('#endif  // !defined(OS_CHROMEOS)');

// eslint-disable-next-line no-var
var CrSettingsPrefUtilV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/pref_util_tests.m.js';
  }
};

TEST_F('CrSettingsPrefUtilV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsResetPageV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/reset_page_test.m.js';
  }
};

TEST_F('CrSettingsResetPageV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsSiteFaviconV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/site_favicon_test.m.js';
  }
};

TEST_F('CrSettingsSiteFaviconV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsSliderV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/settings_slider_tests.m.js';
  }
};

TEST_F('CrSettingsSliderV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsSubpageV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/settings_subpage_test.m.js';
  }
};

TEST_F('CrSettingsSubpageV3Test', 'All', function() {
  mocha.run();
});

GEN('#if !defined(OS_CHROMEOS)');

// eslint-disable-next-line no-var
var CrSettingsSystemPageV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/system_page_tests.m.js';
  }
};

TEST_F('CrSettingsSystemPageV3Test', 'All', function() {
  mocha.run();
});

GEN('#endif  // !defined(OS_CHROMEOS)');

// eslint-disable-next-line no-var
var CrSettingsTextareaV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/settings_textarea_tests.m.js';
  }
};

TEST_F('CrSettingsTextareaV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsToggleButtonV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/settings_toggle_button_tests.m.js';
  }
};

TEST_F('CrSettingsToggleButtonV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsSearchEnginesV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/search_engines_page_test.m.js';
  }
};

TEST_F('CrSettingsSearchEnginesV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsSearchPageV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/search_page_test.m.js';
  }
};

TEST_F('CrSettingsSearchPageV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsOnStartupPageV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/on_startup_page_tests.m.js';
  }
};

TEST_F('CrSettingsOnStartupPageV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsStartupUrlsPageV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/startup_urls_page_test.m.js';
  }
};

TEST_F('CrSettingsStartupUrlsPageV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsAppearancePageV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/appearance_page_test.m.js';
  }
};

TEST_F('CrSettingsAppearancePageV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsAppearanceFontsPageV3Test =
    class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/appearance_fonts_page_test.m.js';
  }
};

TEST_F('CrSettingsAppearanceFontsPageV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsMenuV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/settings_menu_test.m.js';
  }
};

TEST_F('CrSettingsMenuV3Test', 'SettingsMenu', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsPrefsV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/prefs_tests.m.js';
  }
};

TEST_F('CrSettingsPrefsV3Test', 'All', function() {
  mocha.run();
});

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
var CrSettingsLanguagesV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/languages_tests.m.js';
  }
};

TEST_F('CrSettingsLanguagesV3Test', 'All', function() {
  mocha.run();
});

GEN('#if !defined(OS_MACOSX)');

// eslint-disable-next-line no-var
var CrSettingsEditDictionaryPageV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/edit_dictionary_page_test.m.js';
  }
};

TEST_F('CrSettingsEditDictionaryPageV3Test', 'All', function() {
  mocha.run();
});

GEN('#endif  //!defined(OS_MACOSX)');

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
var CrSettingsSearchV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/search_settings_test.m.js';
  }
};

TEST_F('CrSettingsSearchV3Test', 'All', function() {
  mocha.run();
});

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
var CrSettingsBasicPageV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/basic_page_test.m.js';
  }
};

TEST_F('CrSettingsBasicPageV3Test', 'All', function() {
  mocha.run();
});

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
// Times out on Linux CFI. See http://crbug.com/929288.
GEN('#if !defined(NDEBUG) || (defined(OS_LINUX) && defined(IS_CFI))');
GEN('#define MAYBE_MainPageV3 DISABLED_MainPageV3');
GEN('#else');
GEN('#define MAYBE_MainPageV3 MainPageV3');
GEN('#endif');
TEST_F('CrSettingsMainPageV3Test', 'MAYBE_MainPageV3', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsAutofillPageV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/autofill_page_test.m.js';
  }
};

TEST_F('CrSettingsAutofillPageV3Test', 'All', function() {
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

GEN('#if defined(OS_CHROMEOS)');
// eslint-disable-next-line no-var
var CrSettingsPasswordsSectionV3Test_Cros =
    class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/passwords_section_test_cros.m.js';
  }
};

TEST_F('CrSettingsPasswordsSectionV3Test_Cros', 'All', function() {
  mocha.run();
});
GEN('#endif');

// eslint-disable-next-line no-var
var CrSettingsPaymentsSectionV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/payments_section_test.m.js';
  }
};

TEST_F('CrSettingsPaymentsSectionV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsSiteDataDetailsV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/site_data_details_subpage_tests.m.js';
  }
};

TEST_F('CrSettingsSiteDataDetailsV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsSiteListEntryV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/site_list_entry_tests.m.js';
  }
};

TEST_F('CrSettingsSiteListEntryV3Test', 'All', function() {
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

GEN('#if defined(OS_CHROMEOS)');
// eslint-disable-next-line no-var
var CrSettingsSiteListChromeOSV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/site_list_tests_cros.m.js';
  }
};

// Copied from Polymer 2 test:
// TODO(crbug.com/929455): flaky, fix.
TEST_F(
    'CrSettingsSiteListChromeOSV3Test', 'DISABLED_AndroidSmsInfo', function() {
      mocha.run();
    });
GEN('#endif  // defined(OS_CHROMEOS)');

// eslint-disable-next-line no-var
var CrSettingsChooserExceptionListEntryV3Test =
    class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/chooser_exception_list_entry_tests.m.js';
  }
};

TEST_F('CrSettingsChooserExceptionListEntryV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsChooserExceptionListV3Test =
    class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/chooser_exception_list_tests.m.js';
  }
};

TEST_F('CrSettingsChooserExceptionListV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsCategoryDefaultSettingV3Test =
    class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/category_default_setting_tests.m.js';
  }
};

TEST_F('CrSettingsCategoryDefaultSettingV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsCategorySettingExceptionsV3Test =
    class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/category_setting_exceptions_tests.m.js';
  }
};

TEST_F('CrSettingsCategorySettingExceptionsV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsSiteEntryV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/site_entry_tests.m.js';
  }
};

TEST_F('CrSettingsSiteEntryV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsSiteDetailsPermissionV3Test =
    class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/site_details_permission_tests.m.js';
  }
};

TEST_F('CrSettingsSiteDetailsPermissionV3Test', 'All', function() {
  mocha.run();
});
