// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_ISSUE_STORAGE_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_ISSUE_STORAGE_H_

#include <deque>

#include "base/unguessable_token.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {

namespace protocol {
namespace Audits {
class InspectorIssue;
}  // namespace Audits
}  // namespace protocol

class DevToolsIssueStorage
    : public content::WebContentsUserData<DevToolsIssueStorage>,
      public WebContentsObserver {
 public:
  ~DevToolsIssueStorage() override;

  static DevToolsIssueStorage* GetOrCreateForWebContents(
      WebContents* contents) {
    content::WebContentsUserData<DevToolsIssueStorage>::CreateForWebContents(
        contents);
    return FromWebContents(contents);
  }

  void AddInspectorIssue(
      int frame_tree_node_id,
      std::unique_ptr<protocol::Audits::InspectorIssue> issue);
  std::vector<const protocol::Audits::InspectorIssue*> FilterIssuesBy(
      const base::flat_set<int>& frame_tree_node_ids) const;

 private:
  explicit DevToolsIssueStorage(content::WebContents* contents);
  friend class content::WebContentsUserData<DevToolsIssueStorage>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  // WebContentsObserver overrides.
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;
  void FrameDeleted(RenderFrameHost* render_frame_host) override;

  using FrameAssociatedIssue =
      std::pair<int, std::unique_ptr<protocol::Audits::InspectorIssue>>;
  std::deque<FrameAssociatedIssue> issues_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_ISSUE_STORAGE_H_
