// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/user_selectable_type.h"

#include <type_traits>

#include "base/logging.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/pref_names.h"

#if defined(OS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif

namespace syncer {

namespace {

struct UserSelectableTypeInfo {
  const char* const type_name;
  const ModelType canonical_model_type;
  const ModelTypeSet model_type_group;
};

constexpr char kBookmarksTypeName[] = "bookmarks";
constexpr char kPreferencesTypeName[] = "preferences";
constexpr char kPasswordsTypeName[] = "passwords";
constexpr char kAutofillTypeName[] = "autofill";
constexpr char kThemesTypeName[] = "themes";
constexpr char kTypedUrlsTypeName[] = "typedUrls";
constexpr char kExtensionsTypeName[] = "extensions";
constexpr char kAppsTypeName[] = "apps";
constexpr char kReadingListTypeName[] = "readingList";
constexpr char kTabsTypeName[] = "tabs";

UserSelectableTypeInfo GetUserSelectableTypeInfo(UserSelectableType type) {
  // UserSelectableTypeInfo::type_name is used in js code and shouldn't be
  // changed without updating js part.
  switch (type) {
    case UserSelectableType::kBookmarks:
      return {kBookmarksTypeName, BOOKMARKS, {BOOKMARKS}};
    case UserSelectableType::kPreferences: {
      ModelTypeSet model_types = {PREFERENCES, DICTIONARY, PRIORITY_PREFERENCES,
                                  SEARCH_ENGINES};
#if defined(OS_CHROMEOS)
      // SplitSettingsSync makes Printers a separate OS setting.
      if (!chromeos::features::IsSplitSettingsSyncEnabled())
        model_types.Put(PRINTERS);
#endif
      return {kPreferencesTypeName, PREFERENCES, model_types};
    }
    case UserSelectableType::kPasswords:
      return {kPasswordsTypeName, PASSWORDS, {PASSWORDS}};
    case UserSelectableType::kAutofill:
      return {kAutofillTypeName,
              AUTOFILL,
              {AUTOFILL, AUTOFILL_PROFILE, AUTOFILL_WALLET_DATA,
               AUTOFILL_WALLET_METADATA}};
    case UserSelectableType::kThemes:
      return {kThemesTypeName, THEMES, {THEMES}};
    case UserSelectableType::kHistory:
      return {kTypedUrlsTypeName,
              TYPED_URLS,
              {TYPED_URLS, HISTORY_DELETE_DIRECTIVES, SESSIONS, FAVICON_IMAGES,
               FAVICON_TRACKING, USER_EVENTS}};
    case UserSelectableType::kExtensions:
      return {
          kExtensionsTypeName, EXTENSIONS, {EXTENSIONS, EXTENSION_SETTINGS}};
    case UserSelectableType::kApps: {
      ModelTypeSet model_types = {APPS, APP_SETTINGS, WEB_APPS};
#if defined(OS_CHROMEOS)
      // App list must sync if either Chrome apps or ARC apps are synced.
      model_types.Put(APP_LIST);
      // SplitSettingsSync moves ARC apps under a separate OS setting.
      if (!chromeos::features::IsSplitSettingsSyncEnabled())
        model_types.Put(ARC_PACKAGE);
#endif
      return {kAppsTypeName, APPS, model_types};
    }
    case UserSelectableType::kReadingList:
      return {kReadingListTypeName, READING_LIST, {READING_LIST}};
    case UserSelectableType::kTabs:
      return {kTabsTypeName,
              PROXY_TABS,
              {PROXY_TABS, SESSIONS, FAVICON_IMAGES, FAVICON_TRACKING,
               SEND_TAB_TO_SELF}};
  }
  NOTREACHED();
  return {nullptr, UNSPECIFIED};
}

#if defined(OS_CHROMEOS)
UserSelectableTypeInfo GetUserSelectableOsTypeInfo(UserSelectableOsType type) {
  // UserSelectableTypeInfo::type_name is used in js code and shouldn't be
  // changed without updating js part.
  switch (type) {
    case UserSelectableOsType::kOsApps:
      // App list must sync if either Chrome apps or ARC apps are synced.
      return {"osApps", ARC_PACKAGE, {ARC_PACKAGE, APP_LIST}};
    case UserSelectableOsType::kOsPreferences:
      return {"osPreferences",
              OS_PREFERENCES,
              {OS_PREFERENCES, OS_PRIORITY_PREFERENCES}};
    case UserSelectableOsType::kPrinters:
      return {"printers", PRINTERS, {PRINTERS}};
    case UserSelectableOsType::kWifiConfigurations:
      return {"wifiConfigurations", WIFI_CONFIGURATIONS, {WIFI_CONFIGURATIONS}};
  }
}
#endif

}  // namespace

const char* GetUserSelectableTypeName(UserSelectableType type) {
  return GetUserSelectableTypeInfo(type).type_name;
}

UserSelectableType GetUserSelectableTypeFromString(const std::string& type) {
  if (type == kBookmarksTypeName) {
    return UserSelectableType::kBookmarks;
  }
  if (type == kPreferencesTypeName) {
    return UserSelectableType::kPreferences;
  }
  if (type == kPasswordsTypeName) {
    return UserSelectableType::kPasswords;
  }
  if (type == kAutofillTypeName) {
    return UserSelectableType::kAutofill;
  }
  if (type == kThemesTypeName) {
    return UserSelectableType::kThemes;
  }
  if (type == kTypedUrlsTypeName) {
    return UserSelectableType::kHistory;
  }
  if (type == kExtensionsTypeName) {
    return UserSelectableType::kExtensions;
  }
  if (type == kAppsTypeName) {
    return UserSelectableType::kApps;
  }
  if (type == kReadingListTypeName) {
    return UserSelectableType::kReadingList;
  }
  if (type == kTabsTypeName) {
    return UserSelectableType::kTabs;
  }
  NOTREACHED();
  return UserSelectableType::kLastType;
}

std::string UserSelectableTypeSetToString(UserSelectableTypeSet types) {
  std::string result;
  for (UserSelectableType type : types) {
    if (!result.empty()) {
      result += ", ";
    }
    result += GetUserSelectableTypeName(type);
  }
  return result;
}

ModelTypeSet UserSelectableTypeToAllModelTypes(UserSelectableType type) {
  return GetUserSelectableTypeInfo(type).model_type_group;
}

ModelType UserSelectableTypeToCanonicalModelType(UserSelectableType type) {
  return GetUserSelectableTypeInfo(type).canonical_model_type;
}

int UserSelectableTypeToHistogramInt(UserSelectableType type) {
  // TODO(crbug.com/1007293): Use ModelTypeHistogramValue instead of casting to
  // int.
  return static_cast<int>(
      ModelTypeHistogramValue(UserSelectableTypeToCanonicalModelType(type)));
}

#if defined(OS_CHROMEOS)
const char* GetUserSelectableOsTypeName(UserSelectableOsType type) {
  return GetUserSelectableOsTypeInfo(type).type_name;
}

ModelTypeSet UserSelectableOsTypeToAllModelTypes(UserSelectableOsType type) {
  return GetUserSelectableOsTypeInfo(type).model_type_group;
}

ModelType UserSelectableOsTypeToCanonicalModelType(UserSelectableOsType type) {
  return GetUserSelectableOsTypeInfo(type).canonical_model_type;
}
#endif  // defined(OS_CHROMEOS)

}  // namespace syncer
