// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/user_selectable_type.h"

#include <type_traits>

#include "base/logging.h"

namespace syncer {

namespace {

struct UserSelectableTypeInfo {
  const char* const type_name;
  const ModelType canonical_model_type;
  const ModelTypeSet model_type_group;
};

UserSelectableTypeInfo GetUserSelectableTypeInfo(UserSelectableType type) {
  // UserSelectableTypeInfo::type_name is used in js code and shouldn't be
  // changed without updating js part.
  switch (type) {
    case UserSelectableType::kBookmarks:
      return {"bookmarks", BOOKMARKS, {BOOKMARKS}};
    case UserSelectableType::kPreferences:
      return {"preferences",
              PREFERENCES,
              {PREFERENCES, DICTIONARY, PRIORITY_PREFERENCES, SEARCH_ENGINES}};
    case UserSelectableType::kPasswords:
      return {"passwords", PASSWORDS, {PASSWORDS}};
    case UserSelectableType::kAutofill:
      return {"autofill",
              AUTOFILL,
              {AUTOFILL, AUTOFILL_PROFILE, AUTOFILL_WALLET_DATA,
               AUTOFILL_WALLET_METADATA}};
    case UserSelectableType::kThemes:
      return {"themes", THEMES, {THEMES}};
    case UserSelectableType::kHistory:
      return {"typedUrls",
              TYPED_URLS,
              {TYPED_URLS, HISTORY_DELETE_DIRECTIVES, SESSIONS, FAVICON_IMAGES,
               FAVICON_TRACKING, USER_EVENTS}};
    case UserSelectableType::kExtensions:
      return {"extensions", EXTENSIONS, {EXTENSIONS, EXTENSION_SETTINGS}};
    case UserSelectableType::kApps:
      return {
          "apps", APPS, {APPS, APP_SETTINGS, APP_LIST, ARC_PACKAGE, WEB_APPS}};
#if BUILDFLAG(ENABLE_READING_LIST)
    case UserSelectableType::kReadingList:
      return {"readingList", READING_LIST, {READING_LIST}};
#endif
    case UserSelectableType::kTabs:
      return {"tabs",
              PROXY_TABS,
              {PROXY_TABS, SESSIONS, FAVICON_IMAGES, FAVICON_TRACKING,
               SEND_TAB_TO_SELF}};
  }
  NOTREACHED();
  return {nullptr, UNSPECIFIED};
}

}  // namespace

const char* GetUserSelectableTypeName(UserSelectableType type) {
  return GetUserSelectableTypeInfo(type).type_name;
}

ModelTypeSet UserSelectableTypeToAllModelTypes(UserSelectableType type) {
  return GetUserSelectableTypeInfo(type).model_type_group;
}

ModelType UserSelectableTypeToCanonicalModelType(UserSelectableType type) {
  return GetUserSelectableTypeInfo(type).canonical_model_type;
}

int UserSelectableTypeToHistogramInt(UserSelectableType type) {
  return ModelTypeToHistogramInt(UserSelectableTypeToCanonicalModelType(type));
}

}  // namespace syncer
