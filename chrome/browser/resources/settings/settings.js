// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './about_page/about_page.m.js';
import './appearance_page/appearance_page.m.js';
import './autofill_page/autofill_page.m.js';
import './basic_page/basic_page.m.js';
import './controls/controlled_radio_button.m.js';
import './controls/extension_controlled_indicator.m.js';
import './controls/settings_checkbox.m.js';
import './controls/settings_dropdown_menu.m.js';
import './controls/settings_idle_load.m.js';
import './controls/settings_toggle_button.m.js';
import './on_startup_page/on_startup_page.m.js';
import './on_startup_page/startup_urls_page.m.js';
import './prefs/prefs.m.js';
import './privacy_page/privacy_page.m.js';
import './safety_check_page/safety_check_page.m.js';
import './site_favicon.m.js';
import './search_page/search_page.m.js';
import './settings_main/settings_main.m.js';
import './settings_menu/settings_menu.m.js';
import './settings_page/settings_subpage.m.js';
import './settings_page/settings_animated_pages.m.js';
import './settings_ui/settings_ui.m.js';

// <if expr="not chromeos">
import './default_browser_page/default_browser_page.m.js';
// </if>

// <if expr="not chromeos">
export {DefaultBrowserBrowserProxyImpl} from './default_browser_page/default_browser_browser_proxy.m.js';
export {ImportDataBrowserProxyImpl, ImportDataStatus} from './people_page/import_data_browser_proxy.m.js';
// </if>

// <if expr="chromeos">
export {AccountManagerBrowserProxyImpl} from './people_page/account_manager_browser_proxy.m.js';
// </if>

export {AboutPageBrowserProxy, AboutPageBrowserProxyImpl, UpdateStatus} from './about_page/about_page_browser_proxy.m.js';
export {AppearanceBrowserProxy, AppearanceBrowserProxyImpl} from './appearance_page/appearance_browser_proxy.m.js';
export {CrSettingsPrefs} from './prefs/prefs_types.m.js';
export {ExtensionControlBrowserProxyImpl} from './extension_control_browser_proxy.m.js';
export {getSearchManager, SearchRequest, setSearchManagerForTesting} from './search_settings.m.js';
export {HatsBrowserProxyImpl} from './hats_browser_proxy.m.js';
export {LifetimeBrowserProxyImpl} from './lifetime_browser_proxy.m.js';
export {MetricsBrowserProxyImpl, PrivacyElementInteractions, SafetyCheckElementInteractions} from './metrics_browser_proxy.m.js';
export {OnStartupBrowserProxy, OnStartupBrowserProxyImpl} from './on_startup_page/on_startup_browser_proxy.m.js';
export {EDIT_STARTUP_URL_EVENT} from './on_startup_page/startup_url_entry.m.js';
export {StartupUrlsPageBrowserProxy, StartupUrlsPageBrowserProxyImpl} from './on_startup_page/startup_urls_page_browser_proxy.m.js';
export {OpenWindowProxyImpl} from './open_window_proxy.m.js';
export {PageStatus, StatusAction, SyncBrowserProxyImpl} from './people_page/sync_browser_proxy.m.js';
export {ProfileInfoBrowserProxyImpl} from './people_page/profile_info_browser_proxy.m.js';
export {pageVisibility} from './page_visibility.m.js';
export {PasswordManagerProxy, PasswordManagerImpl} from './autofill_page/password_manager_proxy.m.js';
export {prefToString, stringToPrefValue} from './prefs/pref_util.m.js';
export {ResetBrowserProxyImpl} from './reset_page/reset_browser_proxy.m.js';
export {routes} from './route.m.js';
export {Route, Router} from './router.m.js';
export {SearchEnginesBrowserProxyImpl} from './search_engines_page/search_engines_browser_proxy.m.js';
export {PrivacyPageBrowserProxyImpl, SecureDnsMode, SecureDnsUiManagementMode} from './privacy_page/privacy_page_browser_proxy.m.js';
export {SafetyCheckBrowserProxy, SafetyCheckBrowserProxyImpl, SafetyCheckCallbackConstants, SafetyCheckExtensionsStatus, SafetyCheckSafeBrowsingStatus, SafetyCheckPasswordsStatus, SafetyCheckUpdatesStatus} from './safety_check_page/safety_check_browser_proxy.m.js';
