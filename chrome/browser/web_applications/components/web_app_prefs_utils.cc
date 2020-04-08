// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_prefs_utils.h"

#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "services/preferences/public/cpp/dictionary_value_update.h"

namespace web_app {

namespace {

const base::DictionaryValue* GetWebAppDictionary(
    const PrefService* pref_service,
    const AppId& app_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const base::DictionaryValue* web_apps_prefs =
      pref_service->GetDictionary(prefs::kWebAppsPreferences);
  if (!web_apps_prefs)
    return nullptr;

  const base::Value* web_app_prefs = web_apps_prefs->FindDictKey(app_id);
  if (!web_app_prefs)
    return nullptr;

  return &base::Value::AsDictionaryValue(*web_app_prefs);
}

std::unique_ptr<prefs::DictionaryValueUpdate> UpdateWebAppDictionary(
    std::unique_ptr<prefs::DictionaryValueUpdate> web_apps_prefs_update,
    const AppId& app_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::unique_ptr<prefs::DictionaryValueUpdate> web_app_prefs_update;
  if (!web_apps_prefs_update->GetDictionaryWithoutPathExpansion(
          app_id, &web_app_prefs_update)) {
    web_app_prefs_update =
        web_apps_prefs_update->SetDictionaryWithoutPathExpansion(
            app_id, std::make_unique<base::DictionaryValue>());
  }
  return web_app_prefs_update;
}

}  // namespace

// The stored preferences look like:
// "web_apps": {
//   "web_app_ids": {
//     "<app_id_1>": {
//       "was_external_app_uninstalled_by_user": true,
//       "file_handlers_enabled": true,
//       A double representing the number of seconds since epoch, in local time.
//       Convert from/to using base::Time::FromDoubleT() and
//       base::Time::ToDoubleT().
//       "file_handling_origin_trial_expiry_time": 1580475600000
//     },
//     "<app_id_N>": {
//       "was_external_app_uninstalled_by_user": false,
//       "file_handlers_enabled": false,
//       "file_handling_origin_trial_expiry_time": 0
//     }
//   }
// }
//
const char kWasExternalAppUninstalledByUser[] =
    "was_external_app_uninstalled_by_user";

const char kFileHandlersEnabled[] = "file_handlers_enabled";

const char kFileHandlingOriginTrialExpiryTime[] =
    "file_handling_origin_trial_expiry_time";

const char kExperimentalTabbedWindowMode[] = "experimental_tabbed_window_mode";

const char kLatestWebAppInstallSource[] = "latest_web_app_install_source";

void WebAppPrefsUtilsRegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(::prefs::kWebAppsPreferences);
}

bool GetBoolWebAppPref(const PrefService* pref_service,
                       const AppId& app_id,
                       base::StringPiece path) {
  const base::DictionaryValue* web_app_prefs =
      GetWebAppDictionary(pref_service, app_id);
  bool pref_value = false;
  if (web_app_prefs)
    web_app_prefs->GetBoolean(path, &pref_value);
  return pref_value;
}

void UpdateBoolWebAppPref(PrefService* pref_service,
                          const AppId& app_id,
                          base::StringPiece path,
                          bool value) {
  prefs::ScopedDictionaryPrefUpdate update(pref_service,
                                           prefs::kWebAppsPreferences);

  std::unique_ptr<prefs::DictionaryValueUpdate> web_app_prefs =
      UpdateWebAppDictionary(update.Get(), app_id);
  web_app_prefs->SetBoolean(path, value);
}

base::Optional<int> GetIntWebAppPref(const PrefService* pref_service,
                                     const AppId& app_id,
                                     base::StringPiece path) {
  const base::DictionaryValue* web_app_prefs =
      GetWebAppDictionary(pref_service, app_id);
  if (web_app_prefs)
    return web_app_prefs->FindIntPath(path);
  return base::nullopt;
}

void UpdateIntWebAppPref(PrefService* pref_service,
                         const AppId& app_id,
                         base::StringPiece path,
                         int value) {
  prefs::ScopedDictionaryPrefUpdate update(pref_service,
                                           prefs::kWebAppsPreferences);

  std::unique_ptr<prefs::DictionaryValueUpdate> web_app_prefs =
      UpdateWebAppDictionary(update.Get(), app_id);
  web_app_prefs->SetInteger(path, value);
}

base::Optional<double> GetDoubleWebAppPref(const PrefService* pref_service,
                                           const AppId& app_id,
                                           base::StringPiece path) {
  const base::DictionaryValue* web_app_prefs =
      GetWebAppDictionary(pref_service, app_id);
  if (web_app_prefs)
    return web_app_prefs->FindDoublePath(path);
  return base::nullopt;
}

void UpdateDoubleWebAppPref(PrefService* pref_service,
                            const AppId& app_id,
                            base::StringPiece path,
                            double value) {
  prefs::ScopedDictionaryPrefUpdate update(pref_service,
                                           prefs::kWebAppsPreferences);

  std::unique_ptr<prefs::DictionaryValueUpdate> web_app_prefs =
      UpdateWebAppDictionary(update.Get(), app_id);
  web_app_prefs->SetDouble(path, value);
}

void RemoveWebAppPref(PrefService* pref_service,
                      const AppId& app_id,
                      base::StringPiece path) {
  prefs::ScopedDictionaryPrefUpdate update(pref_service,
                                           prefs::kWebAppsPreferences);

  std::unique_ptr<prefs::DictionaryValueUpdate> web_app_prefs =
      UpdateWebAppDictionary(update.Get(), app_id);
  web_app_prefs->Remove(path, nullptr);
}

}  // namespace web_app
