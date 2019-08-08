// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOOKALIKES_SAFETY_TIPS_REPUTATION_WEB_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_LOOKALIKES_SAFETY_TIPS_REPUTATION_WEB_CONTENTS_OBSERVER_H_

#include <set>

#include "chrome/browser/lookalikes/safety_tips/reputation_service.h"
#include "chrome/browser/lookalikes/safety_tips/safety_tip_ui.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

class Profile;

namespace safety_tips {

// Observes navigations and triggers a warning if a visited site is determined
// to be low-reputation as determined by heuristics or inclusion on
// pre-calculated lists.
class ReputationWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ReputationWebContentsObserver> {
 public:
  ~ReputationWebContentsObserver() override;

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  SafetyTipType last_shown_safety_tip_type() const {
    return last_shown_safety_tip_type_;
  }

 private:
  friend class content::WebContentsUserData<ReputationWebContentsObserver>;

  explicit ReputationWebContentsObserver(content::WebContents* web_contents);

  // A ReputationCheckCallback. Called by the reputation service when a
  // reputation result is available.
  void HandleReputationCheckResult(safety_tips::SafetyTipType type,
                                   bool user_ignored,
                                   const GURL& url);

  Profile* profile_;
  // Used to cache the last shown safety tip type so that Page Info can fetch
  // this information without performing a reputation check. Resets to kNone on
  // new top frame navigations.
  safety_tips::SafetyTipType last_shown_safety_tip_type_ = SafetyTipType::kNone;

  base::WeakPtrFactory<ReputationWebContentsObserver> weak_factory_;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace safety_tips

#endif  // CHROME_BROWSER_LOOKALIKES_SAFETY_TIPS_REPUTATION_WEB_CONTENTS_OBSERVER_H_
