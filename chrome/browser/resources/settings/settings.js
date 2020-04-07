// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './settings_ui/settings_ui.m.js';

export {AboutPageBrowserProxy, AboutPageBrowserProxyImpl, UpdateStatus} from './about_page/about_page_browser_proxy.m.js';
export {AppearanceBrowserProxy, AppearanceBrowserProxyImpl} from './appearance_page/appearance_browser_proxy.m.js';
export {PasswordManagerImpl, PasswordManagerProxy} from './autofill_page/password_manager_proxy.m.js';
// <if expr="not chromeos">
export {DefaultBrowserBrowserProxyImpl} from './default_browser_page/default_browser_browser_proxy.m.js';
// </if>
export {ExtensionControlBrowserProxyImpl} from './extension_control_browser_proxy.m.js';
export {HatsBrowserProxyImpl} from './hats_browser_proxy.m.js';
export {LifetimeBrowserProxyImpl} from './lifetime_browser_proxy.m.js';
export {MetricsBrowserProxyImpl, PrivacyElementInteractions, SafetyCheckInteractions} from './metrics_browser_proxy.m.js';
export {OnStartupBrowserProxy, OnStartupBrowserProxyImpl} from './on_startup_page/on_startup_browser_proxy.m.js';
export {EDIT_STARTUP_URL_EVENT} from './on_startup_page/startup_url_entry.m.js';
export {StartupUrlsPageBrowserProxy, StartupUrlsPageBrowserProxyImpl} from './on_startup_page/startup_urls_page_browser_proxy.m.js';
export {OpenWindowProxyImpl} from './open_window_proxy.m.js';
export {pageVisibility, setPageVisibilityForTesting} from './page_visibility.m.js';
// <if expr="chromeos">
export {AccountManagerBrowserProxyImpl} from './people_page/account_manager_browser_proxy.m.js';
// </if>
// <if expr="not chromeos">
export {ImportDataBrowserProxyImpl, ImportDataStatus} from './people_page/import_data_browser_proxy.m.js';
// </if>
export {ProfileInfoBrowserProxyImpl} from './people_page/profile_info_browser_proxy.m.js';
export {MAX_SIGNIN_PROMO_IMPRESSION} from './people_page/sync_account_control.m.js';
export {PageStatus, StatusAction, SyncBrowserProxyImpl} from './people_page/sync_browser_proxy.m.js';
export {PluralStringProxyImpl} from './plural_string_proxy.m.js';
export {prefToString, stringToPrefValue} from './prefs/pref_util.m.js';
export {CrSettingsPrefs} from './prefs/prefs_types.m.js';
export {PrivacyPageBrowserProxyImpl, SecureDnsMode, SecureDnsUiManagementMode} from './privacy_page/privacy_page_browser_proxy.m.js';
export {ResetBrowserProxyImpl} from './reset_page/reset_browser_proxy.m.js';
export {buildRouter, routes} from './route.m.js';
export {Route, Router} from './router.m.js';
export {SafetyCheckBrowserProxy, SafetyCheckBrowserProxyImpl, SafetyCheckCallbackConstants, SafetyCheckExtensionsStatus, SafetyCheckPasswordsStatus, SafetyCheckSafeBrowsingStatus, SafetyCheckUpdatesStatus} from './safety_check_page/safety_check_browser_proxy.m.js';
export {SearchEnginesBrowserProxyImpl} from './search_engines_page/search_engines_browser_proxy.m.js';
export {getSearchManager, SearchRequest, setSearchManagerForTesting} from './search_settings.m.js';
