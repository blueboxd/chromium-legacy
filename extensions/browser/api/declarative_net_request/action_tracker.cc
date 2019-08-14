// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/action_tracker.h"

#include "base/stl_util.h"
#include "extensions/browser/api/declarative_net_request/rules_monitor_service.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/constants.h"

namespace extensions {
namespace declarative_net_request {

ActionTracker::ActionTracker(content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  extension_prefs_ = ExtensionPrefs::Get(browser_context_);
}

ActionTracker::~ActionTracker() {
  DCHECK(actions_matched_.empty());
}

void ActionTracker::OnRuleMatched(const std::vector<ExtensionId>& extension_ids,
                                  int tab_id) {
  if (tab_id == extension_misc::kUnknownTabId)
    return;

  for (const auto& extension_id : extension_ids) {
    auto key = std::make_pair(extension_id, tab_id);
    int action_count = ++actions_matched_[key];

    if (extension_prefs_->GetDNRUseActionCountAsBadgeText(extension_id)) {
      DCHECK(ExtensionsAPIClient::Get());
      ExtensionsAPIClient::Get()->UpdateActionCount(
          browser_context_, extension_id, tab_id, action_count);
    }
  }
}

void ActionTracker::ClearExtensionData(const ExtensionId& extension_id) {
  auto compare_by_extension_id =
      [&extension_id](const std::pair<ExtensionTabKey, size_t>& it) {
        return it.first.first == extension_id;
      };

  base::EraseIf(actions_matched_, compare_by_extension_id);
}

void ActionTracker::ClearTabData(int tab_id) {
  auto compare_by_tab_id =
      [&tab_id](const std::pair<ExtensionTabKey, size_t>& it) {
        return it.first.second == tab_id;
      };

  base::EraseIf(actions_matched_, compare_by_tab_id);
}

void ActionTracker::ResetActionCountForTab(int tab_id) {
  RulesMonitorService* rules_monitor_service =
      RulesMonitorService::Get(browser_context_);

  DCHECK(rules_monitor_service);
  for (const auto& extension_id :
       rules_monitor_service->extensions_with_rulesets()) {
    auto key = std::make_pair(extension_id, tab_id);
    actions_matched_[key] = 0;

    if (extension_prefs_->GetDNRUseActionCountAsBadgeText(extension_id)) {
      DCHECK(ExtensionsAPIClient::Get());
      ExtensionsAPIClient::Get()->UpdateActionCount(
          browser_context_, extension_id, tab_id, 0 /* action_count */);
    }
  }
}

}  // namespace declarative_net_request
}  // namespace extensions
