// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager.h"

#include "base/stl_util.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace policy {

static DlpContentManager* g_dlp_content_manager = nullptr;

// static
DlpContentManager* DlpContentManager::Get() {
  if (!g_dlp_content_manager)
    g_dlp_content_manager = new DlpContentManager();
  return g_dlp_content_manager;
}

DlpContentRestrictionSet DlpContentManager::GetConfidentialRestrictions(
    const content::WebContents* web_contents) const {
  if (!base::Contains(confidential_web_contents_, web_contents))
    return DlpContentRestrictionSet();
  return confidential_web_contents_.at(web_contents);
}

DlpContentRestrictionSet DlpContentManager::GetOnScreenPresentRestrictions()
    const {
  return on_screen_restrictions_;
}

/* static */
void DlpContentManager::SetDlpContentManagerForTesting(
    DlpContentManager* dlp_content_manager) {
  if (g_dlp_content_manager)
    delete g_dlp_content_manager;
  g_dlp_content_manager = dlp_content_manager;
}

/* static */
void DlpContentManager::ResetDlpContentManagerForTesting() {
  g_dlp_content_manager = nullptr;
}

DlpContentManager::DlpContentManager() = default;

DlpContentManager::~DlpContentManager() = default;

void DlpContentManager::OnConfidentialityChanged(
    content::WebContents* web_contents,
    const DlpContentRestrictionSet& restriction_set) {
  if (restriction_set.IsEmpty()) {
    RemoveFromConfidential(web_contents);
  } else {
    confidential_web_contents_[web_contents] = restriction_set;
    if (web_contents->GetVisibility() == content::Visibility::VISIBLE) {
      MaybeChangeOnScreenRestrictions();
    }
  }
}

void DlpContentManager::OnWebContentsDestroyed(
    const content::WebContents* web_contents) {
  RemoveFromConfidential(web_contents);
}

DlpContentRestrictionSet DlpContentManager::GetRestrictionSetForURL(
    const GURL& url) const {
  DlpContentRestrictionSet set;
  // TODO(crbug/1109783): Implement based on the policy.
  return set;
}

void DlpContentManager::OnVisibilityChanged(
    content::WebContents* web_contents) {
  MaybeChangeOnScreenRestrictions();
}

void DlpContentManager::RemoveFromConfidential(
    const content::WebContents* web_contents) {
  confidential_web_contents_.erase(web_contents);
  MaybeChangeOnScreenRestrictions();
}

void DlpContentManager::MaybeChangeOnScreenRestrictions() {
  DlpContentRestrictionSet new_restriction_set;
  // TODO(crbug/1111860): Recalculate more effectively.
  for (const auto& entry : confidential_web_contents_) {
    if (entry.first->GetVisibility() == content::Visibility::VISIBLE) {
      new_restriction_set.UnionWith(entry.second);
    }
  }
  if (on_screen_restrictions_ != new_restriction_set) {
    on_screen_restrictions_ = new_restriction_set;
    OnScreenRestrictionsChanged(on_screen_restrictions_);
  }
}

void DlpContentManager::OnScreenRestrictionsChanged(
    const DlpContentRestrictionSet& restrictions) const {
  // TODO(crbug/1105991): Implement enforcing/releasing of restrictions.
}

}  // namespace policy
