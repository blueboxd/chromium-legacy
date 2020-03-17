// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './a11y_page/a11y_page.m.js';
import './about_page/about_page.m.js';
import './appearance_page/appearance_page.m.js';
import './appearance_page/appearance_fonts_page.m.js';
import './autofill_page/autofill_page.m.js';
import './autofill_page/password_check.m.js';
import './autofill_page/passwords_section.m.js';
import './basic_page/basic_page.m.js';
import './clear_browsing_data_dialog/clear_browsing_data_dialog.m.js';
import './controls/controlled_button.m.js';
import './controls/controlled_radio_button.m.js';
import './controls/extension_controlled_indicator.m.js';
import './controls/settings_checkbox.m.js';
import './controls/settings_dropdown_menu.m.js';
import './controls/settings_idle_load.m.js';
import './controls/settings_slider.m.js';
import './controls/settings_textarea.m.js';
import './controls/settings_toggle_button.m.js';
import './downloads_page/downloads_page.m.js';
import './on_startup_page/on_startup_page.m.js';
import './on_startup_page/startup_urls_page.m.js';
import './prefs/prefs.m.js';
import './printing_page/printing_page.m.js';
import './reset_page/reset_page.m.js';
import './site_favicon.m.js';
import './search_engines_page/omnibox_extension_entry.m.js';
import './search_engines_page/search_engine_dialog.m.js';
import './search_engines_page/search_engine_entry.m.js';
import './search_engines_page/search_engines_page.m.js';
import './search_page/search_page.m.js';
import './settings_main/settings_main.m.js';
import './settings_menu/settings_menu.m.js';
import './settings_page/settings_subpage.m.js';
import './settings_page/settings_animated_pages.m.js';
import './settings_ui/settings_ui.m.js';

// <if expr="_google_chrome and is_win">
import './incompatible_applications_page/incompatible_applications_page.m.js';
// </if>

// <if expr="not chromeos">
import './default_browser_page/default_browser_page.m.js';
import './people_page/import_data_dialog.m.js';
import './system_page/system_page.m.js';
// </if>

// <if expr="not is_macosx">
import './languages_page/edit_dictionary_page.m.js';
// </if>

export {getToastManager} from 'chrome://resources/cr_elements/cr_toast/cr_toast_manager.m.js';

// <if expr="_google_chrome and is_win">
export {CHROME_CLEANUP_DEFAULT_ITEMS_TO_SHOW} from './chrome_cleanup_page/items_to_remove_list.m.js';
export {ChromeCleanupIdleReason} from './chrome_cleanup_page/chrome_cleanup_page.m.js';
export {ChromeCleanupProxyImpl} from './chrome_cleanup_page/chrome_cleanup_proxy.m.js';
export {IncompatibleApplication, IncompatibleApplicationsBrowserProxyImpl} from './incompatible_applications_page/incompatible_applications_browser_proxy.m.js';
// </if>

// <if expr="not chromeos">
export {DefaultBrowserBrowserProxyImpl} from './default_browser_page/default_browser_browser_proxy.m.js';
export {SystemPageBrowserProxyImpl} from './system_page/system_page_browser_proxy.m.js';
export {ImportDataBrowserProxyImpl, ImportDataStatus} from './people_page/import_data_browser_proxy.m.js';
// </if>

// <if expr="chromeos">
export {BlockingRequestManager} from './autofill_page/blocking_request_manager.m.js';
// </if>

export {AboutPageBrowserProxy, AboutPageBrowserProxyImpl, UpdateStatus} from './about_page/about_page_browser_proxy.m.js';
export {AppearanceBrowserProxy, AppearanceBrowserProxyImpl} from './appearance_page/appearance_browser_proxy.m.js';
export {AutofillManagerImpl} from './autofill_page/autofill_section.m.js';
export {ClearBrowsingDataBrowserProxyImpl} from './clear_browsing_data_dialog/clear_browsing_data_browser_proxy.m.js';
export {CountryDetailManagerImpl} from './autofill_page/address_edit_dialog.m.js';
export {CrSettingsPrefs} from './prefs/prefs_types.m.js';
export {DownloadsBrowserProxyImpl} from './downloads_page/downloads_browser_proxy.m.js';
export {ExtensionControlBrowserProxyImpl} from './extension_control_browser_proxy.m.js';
export {FontsBrowserProxy, FontsBrowserProxyImpl} from './appearance_page/fonts_browser_proxy.m.js';
export {getSearchManager, SearchRequest, setSearchManagerForTesting} from './search_settings.m.js';
export {kMenuCloseDelay} from './languages_page/languages_page.m.js';
export {LanguagesBrowserProxyImpl} from './languages_page/languages_browser_proxy.m.js';
export {LifetimeBrowserProxyImpl} from './lifetime_browser_proxy.m.js';
export {MetricsBrowserProxyImpl, PrivacyElementInteractions} from './metrics_browser_proxy.m.js';
export {OnStartupBrowserProxy, OnStartupBrowserProxyImpl} from './on_startup_page/on_startup_browser_proxy.m.js';
export {EDIT_STARTUP_URL_EVENT} from './on_startup_page/startup_url_entry.m.js';
export {StartupUrlsPageBrowserProxy, StartupUrlsPageBrowserProxyImpl} from './on_startup_page/startup_urls_page_browser_proxy.m.js';
export {OpenWindowProxyImpl} from './open_window_proxy.m.js';
export {PageStatus, StatusAction, SyncBrowserProxyImpl} from './people_page/sync_browser_proxy.m.js';
export {pageVisibility} from './page_visibility.m.js';
export {PasswordManagerImpl} from './autofill_page/password_manager_proxy.m.js';
export {PaymentsManagerImpl} from './autofill_page/payments_section.m.js';
export {prefToString, stringToPrefValue} from './prefs/pref_util.m.js';
export {routes} from './route.m.js';
export {ResetBrowserProxyImpl} from './reset_page/reset_browser_proxy.m.js';
export {Route, Router} from './router.m.js';
export {SearchEnginesBrowserProxyImpl} from './search_engines_page/search_engines_browser_proxy.m.js';
