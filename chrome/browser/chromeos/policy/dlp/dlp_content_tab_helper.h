// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_TAB_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_TAB_HELPER_H_

#include "base/containers/flat_set.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace policy {

// DlpContentTabHelper attaches to relevant WebContents that are covered by
// DLP (Data Leak Prevention) feature and observes navigation in all sub-frames
// as well as visibility of the WebContents and reports it to system-wide
// DlpContentManager.
// WebContents is considered as confidential if either the main frame or any
// of sub-frames are confidential according to the current policy.
class DlpContentTabHelper
    : public content::WebContentsUserData<DlpContentTabHelper>,
      public content::WebContentsObserver {
 public:
  ~DlpContentTabHelper() override;

  // content::WebContentsObserver:
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void WebContentsDestroyed() override;
  void OnVisibilityChanged(content::Visibility visibility) override;

 private:
  friend class content::WebContentsUserData<DlpContentTabHelper>;

  explicit DlpContentTabHelper(content::WebContents* web_contents);
  DlpContentTabHelper(const DlpContentTabHelper&) = delete;
  DlpContentTabHelper& operator=(const DlpContentTabHelper&) = delete;

  // WebContents is considered as confidential if either the main frame or any
  // of sub-frames are confidential.
  bool IsConfidential() const;

  // Set of the currently known confidential frames.
  base::flat_set<content::RenderFrameHost*> confidential_frames_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_TAB_HELPER_H_
