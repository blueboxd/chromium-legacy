// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/app_service/public/cpp/preferred_apps_list.h"

#include <utility>

#include "base/strings/string_util.h"
#include "chrome/services/app_service/public/cpp/intent_filter_util.h"
#include "chrome/services/app_service/public/cpp/intent_util.h"
#include "url/gurl.h"

namespace {

void Clone(apps::PreferredAppsList::PreferredApps& source,
           apps::PreferredAppsList::PreferredApps* destination) {
  destination->clear();
  for (auto& preferred_app : source) {
    destination->push_back(preferred_app->Clone());
  }
}

}  // namespace

namespace apps {

PreferredAppsList::PreferredAppsList() = default;
PreferredAppsList::~PreferredAppsList() = default;

// static
apps::mojom::ReplacedAppPreferencesPtr PreferredAppsList::AddPreferredApp(
    const std::string& app_id,
    const apps::mojom::IntentFilterPtr& intent_filter,
    PreferredApps* preferred_apps) {
  auto replaced_app_preferences = apps::mojom::ReplacedAppPreferences::New();
  auto iter = preferred_apps->begin();
  auto& replaced_preference_map = replaced_app_preferences->replaced_preference;

  // Go through the list and see if there are overlapped intent filters in the
  // list. If there is, add this into the replaced_app_preferences and remove it
  // from the list.
  while (iter != preferred_apps->end()) {
    if (apps_util::FiltersHaveOverlap((*iter)->intent_filter, intent_filter)) {
      // Add the to be removed preferred app into a map, key by app_id.
      const std::string replaced_app_id = (*iter)->app_id;
      auto entry = replaced_preference_map.find(replaced_app_id);
      if (entry == replaced_preference_map.end()) {
        std::vector<apps::mojom::IntentFilterPtr> intent_filter_vector;
        intent_filter_vector.push_back((*iter)->intent_filter->Clone());
        replaced_preference_map.emplace(replaced_app_id,
                                        std::move(intent_filter_vector));
      } else {
        entry->second.push_back((*iter)->intent_filter->Clone());
      }
      iter = preferred_apps->erase(iter);
    } else {
      iter++;
    }
  }
  auto new_preferred_app =
      apps::mojom::PreferredApp::New(intent_filter->Clone(), app_id);
  preferred_apps->push_back(std::move(new_preferred_app));
  return replaced_app_preferences;
}

// static
void PreferredAppsList::DeletePreferredApp(
    const std::string& app_id,
    const apps::mojom::IntentFilterPtr& intent_filter,
    PreferredApps* preferred_apps) {
  // Go through the list and see if there are overlapped intent filters with the
  // same app id in the list. If there are, delete the entry.
  auto iter = preferred_apps->begin();
  while (iter != preferred_apps->end()) {
    if ((*iter)->app_id == app_id &&
        apps_util::FiltersHaveOverlap((*iter)->intent_filter, intent_filter)) {
      iter = preferred_apps->erase(iter);
    } else {
      iter++;
    }
  }
}

// static
void PreferredAppsList::DeleteAppId(const std::string& app_id,
                                    PreferredApps* preferred_apps) {
  auto iter = preferred_apps->begin();
  // Go through the list and delete the entry with requested app_id.
  while (iter != preferred_apps->end()) {
    if ((*iter)->app_id == app_id) {
      iter = preferred_apps->erase(iter);
    } else {
      iter++;
    }
  }
}

base::Optional<std::string> PreferredAppsList::FindPreferredAppForIntent(
    const apps::mojom::IntentPtr& intent) {
  base::Optional<std::string> best_match_app_id = base::nullopt;
  int best_match_level = apps_util::IntentFilterMatchLevel::kNone;
  for (auto& preferred_app : preferred_apps_) {
    if (apps_util::IntentMatchesFilter(intent, preferred_app->intent_filter)) {
      int match_level =
          apps_util::GetFilterMatchLevel(preferred_app->intent_filter);
      if (match_level < best_match_level) {
        continue;
      }
      best_match_level = match_level;
      best_match_app_id = preferred_app->app_id;
    }
  }
  return best_match_app_id;
}

base::Optional<std::string> PreferredAppsList::FindPreferredAppForUrl(
    const GURL& url) {
  auto intent = apps_util::CreateIntentFromUrl(url);
  return FindPreferredAppForIntent(intent);
}

apps::mojom::ReplacedAppPreferencesPtr PreferredAppsList::AddPreferredApp(
    const std::string& app_id,
    const apps::mojom::IntentFilterPtr& intent_filter) {
  return AddPreferredApp(app_id, intent_filter, &preferred_apps_);
}

void PreferredAppsList::DeletePreferredApp(
    const std::string& app_id,
    const apps::mojom::IntentFilterPtr& intent_filter) {
  DeletePreferredApp(app_id, intent_filter, &preferred_apps_);
}

void PreferredAppsList::DeleteAppId(const std::string& app_id) {
  DeleteAppId(app_id, &preferred_apps_);
}

void PreferredAppsList::Init() {
  initialized_ = true;
}

void PreferredAppsList::Init(PreferredApps& preferred_apps) {
  Clone(preferred_apps, &preferred_apps_);
  initialized_ = true;
}

PreferredAppsList::PreferredApps PreferredAppsList::GetValue() {
  PreferredAppsList::PreferredApps preferred_apps_copy;
  Clone(preferred_apps_, &preferred_apps_copy);
  return preferred_apps_copy;
}

bool PreferredAppsList::IsInitialized() {
  return initialized_;
}

}  // namespace apps
