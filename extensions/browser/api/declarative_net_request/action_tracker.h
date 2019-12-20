// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_ACTION_TRACKER_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_ACTION_TRACKER_H_

#include <map>
#include <vector>

#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/extension_id.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class ExtensionPrefs;
struct WebRequestInfo;

namespace declarative_net_request {
struct RequestAction;

class ActionTracker {
 public:
  explicit ActionTracker(content::BrowserContext* browser_context);
  ~ActionTracker();
  ActionTracker(const ActionTracker& other) = delete;
  ActionTracker& operator=(const ActionTracker& other) = delete;

  // Called whenever a request matches with a rule.
  void OnRuleMatched(const RequestAction& request_action,
                     const WebRequestInfo& request_info);

  // Updates the action count for all tabs for the specified |extension_id|'s
  // extension action. Called when chrome.setActionCountAsBadgeText(true) is
  // called by an extension.
  void OnPreferenceEnabled(const ExtensionId& extension_id) const;

  // Clears the action count for the specified |extension_id| for all tabs.
  // Called when an extension's ruleset is removed.
  void ClearExtensionData(const ExtensionId& extension_id);

  // Clears the action count for every extension for the specified |tab_id|.
  // Called when the tab has been closed.
  void ClearTabData(int tab_id);

  // Clears the pending action count for every extension in
  // |pending_navigation_actions_| for the specified |navigation_id|.
  void ClearPendingNavigation(int64_t navigation_id);

  // Called when a main-frame navigation to a different document commits.
  // Updates the badge count for all extensions for the given |tab_id|.
  void ResetActionCountForTab(int tab_id, int64_t navigation_id);

 private:
  // Template key type used for TrackedInfo, specified by an extension_id and
  // another ID.
  template <typename T>
  struct TrackedInfoContextKey {
    TrackedInfoContextKey(ExtensionId extension_id, T secondary_id);
    TrackedInfoContextKey(const TrackedInfoContextKey& other) = delete;
    TrackedInfoContextKey& operator=(const TrackedInfoContextKey& other) =
        delete;
    TrackedInfoContextKey(TrackedInfoContextKey&&);
    TrackedInfoContextKey& operator=(TrackedInfoContextKey&&);

    ExtensionId extension_id;
    T secondary_id;

    bool operator<(const TrackedInfoContextKey& other) const;
  };

  using ExtensionTabIdKey = TrackedInfoContextKey<int>;
  using ExtensionNavigationIdKey = TrackedInfoContextKey<int64_t>;

  // Info tracked for each ExtensionTabIdKey or ExtensionNavigationIdKey.
  struct TrackedInfo {
    size_t action_count = 0;
  };

  // Called from OnRuleMatched. Dispatches a OnRuleMatchedDebug event to the
  // observer for the extension specified by |request_action.extension_id|.
  void DispatchOnRuleMatchedDebugIfNeeded(
      const RequestAction& request_action,
      api::declarative_net_request::RequestDetails request_details);

  // Maps a pair of (extension ID, tab ID) to the number of actions matched for
  // the extension and tab specified.
  std::map<ExtensionTabIdKey, TrackedInfo> actions_matched_;

  // Maps a pair of (extension ID, navigation ID) to the number of actions
  // matched for the main-frame request associated with the navigation ID in the
  // key. These actions are added to |actions_matched_| once the navigation
  // commits.
  std::map<ExtensionNavigationIdKey, TrackedInfo> pending_navigation_actions_;

  content::BrowserContext* browser_context_;

  ExtensionPrefs* extension_prefs_;
};

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_ACTION_TRACKER_H_
