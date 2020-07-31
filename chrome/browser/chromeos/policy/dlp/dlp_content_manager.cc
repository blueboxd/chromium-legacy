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

bool DlpContentManager::IsWebContentsConfidential(
    const content::WebContents* web_contents) const {
  return base::Contains(confidential_web_contents_, web_contents);
}

bool DlpContentManager::IsConfidentialDataPresentOnScreen() const {
  return is_confidential_web_contents_visible_;
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
    bool confidential) {
  if (confidential) {
    AddToConfidential(web_contents);
  } else {
    RemoveFromConfidential(web_contents);
  }
}

void DlpContentManager::OnWebContentsDestroyed(
    const content::WebContents* web_contents) {
  RemoveFromConfidential(web_contents);
}

bool DlpContentManager::IsURLConfidential(const GURL& url) const {
  // TODO(crbug/1109783): Implement based on the policy.
  return false;
}

void DlpContentManager::OnVisibilityChanged(content::WebContents* web_contents,
                                            bool visible) {
  MaybeChangeVisibilityFlag();
}

void DlpContentManager::AddToConfidential(content::WebContents* web_contents) {
  confidential_web_contents_.insert(web_contents);
  if (web_contents->GetVisibility() == content::Visibility::VISIBLE) {
    MaybeChangeVisibilityFlag();
  }
}

void DlpContentManager::RemoveFromConfidential(
    const content::WebContents* web_contents) {
  confidential_web_contents_.erase(web_contents);
  MaybeChangeVisibilityFlag();
}

void DlpContentManager::MaybeChangeVisibilityFlag() {
  bool is_confidential_web_contents_currently_visible = false;
  for (auto* web_contents : confidential_web_contents_) {
    if (web_contents->GetVisibility() == content::Visibility::VISIBLE) {
      is_confidential_web_contents_currently_visible = true;
      break;
    }
  }
  if (is_confidential_web_contents_visible_ !=
      is_confidential_web_contents_currently_visible) {
    is_confidential_web_contents_visible_ =
        is_confidential_web_contents_currently_visible;
    OnScreenConfidentialityStateChanged(is_confidential_web_contents_visible_);
  }
}

void DlpContentManager::OnScreenConfidentialityStateChanged(bool visible) {
  // TODO(crbug/1105991): Implement enforcing/releasing of restrictions.
}

}  // namespace policy
