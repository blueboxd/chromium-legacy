// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_MANAGER_H_

#include "base/containers/flat_set.h"

class GURL;

namespace content {
class WebContents;
}  // namespace content

namespace policy {

// System-wide class that tracks the set of currently known confidential
// WebContents and whether any of them are currently visible.
// If any confidential WebContents is visible, the corresponding restrictions
// will be enforced according to the current enterprise policy.
class DlpContentManager {
 public:
  // Creates the instance if not yet created.
  // There will always be a single instance created on the first access.
  static DlpContentManager* Get();

  // Checks whether |web_contents| is confidential according to the policy.
  bool IsWebContentsConfidential(
      const content::WebContents* web_contents) const;

  // Returns whether any WebContents with a confidential content is currently
  // visible.
  bool IsConfidentialDataPresentOnScreen() const;

  // The caller (test) should manage |dlp_content_manager| lifetime.
  // Reset doesn't delete the object.
  static void SetDlpContentManagerForTesting(
      DlpContentManager* dlp_content_manager);
  static void ResetDlpContentManagerForTesting();

 private:
  friend class DlpContentManagerTest;
  friend class DlpContentTabHelper;
  friend class MockDlpContentManager;

  DlpContentManager();
  virtual ~DlpContentManager();
  DlpContentManager(const DlpContentManager&) = delete;
  DlpContentManager& operator=(const DlpContentManager&) = delete;

  // Called from DlpContentTabHelper:
  // Being called when confidentiality state changes for |web_contents|, e.g.
  // because of navigation.
  virtual void OnConfidentialityChanged(content::WebContents* web_contents,
                                        bool confidential);
  // Called when |web_contents| is about to be destroyed.
  virtual void OnWebContentsDestroyed(const content::WebContents* web_contents);
  // Should return whether |url| is considered as confidential according to
  // the policies.
  virtual bool IsURLConfidential(const GURL& url) const;
  // Called when |web_contents| becomes visible or not.
  virtual void OnVisibilityChanged(content::WebContents* web_contents,
                                   bool visible);

  // Helpers to add/remove WebContents from confidential sets.
  void AddToConfidential(content::WebContents* web_contents);
  void RemoveFromConfidential(const content::WebContents* web_contents);

  // Updates |is_confidential_web_contents_visible_| and calls
  // OnScreenConfidentialityStateChanged() if needed.
  void MaybeChangeVisibilityFlag();

  // Called when a confidential content becomes visible or all confidential
  // content becomes not visible.
  void OnScreenConfidentialityStateChanged(bool visible);

  // Set of currently known confidential WebContents.
  base::flat_set<content::WebContents*> confidential_web_contents_;
  // Flag the indicates whether any confidential WebContents is currently
  // visible or not.
  bool is_confidential_web_contents_visible_ = false;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_MANAGER_H_
