// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_USER_SELECTABLE_TYPE_H_
#define COMPONENTS_SYNC_BASE_USER_SELECTABLE_TYPE_H_

#include "components/reading_list/features/reading_list_buildflags.h"
#include "components/sync/base/enum_set.h"
#include "components/sync/base/model_type.h"

namespace syncer {

enum class UserSelectableType {
  kBookmarks,
  kFirstType = kBookmarks,

  kPreferences,
  kPasswords,
  kAutofill,
  kThemes,
  kHistory,
  kExtensions,
  kApps,
// TODO(crbug.com/950874): remove this usage of ENABLE_READING_LIST build
// flag.
#if BUILDFLAG(ENABLE_READING_LIST)
  kReadingList,
#endif
  kWifiConfigurations,
  kTabs,
  kLastType = kTabs
};

using UserSelectableTypeSet = EnumSet<UserSelectableType,
                                      UserSelectableType::kFirstType,
                                      UserSelectableType::kLastType>;

const char* GetUserSelectableTypeName(UserSelectableType type);
ModelTypeSet UserSelectableTypeToAllModelTypes(UserSelectableType type);

ModelType UserSelectableTypeToCanonicalModelType(UserSelectableType type);
int UserSelectableTypeToHistogramInt(UserSelectableType type);

constexpr int UserSelectableTypeHistogramNumEntries() {
  return static_cast<int>(ModelType::NUM_ENTRIES);
}

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_USER_SELECTABLE_TYPE_H_
