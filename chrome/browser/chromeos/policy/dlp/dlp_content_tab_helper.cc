// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_content_tab_helper.h"

#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace policy {

DlpContentTabHelper::~DlpContentTabHelper() = default;

void DlpContentTabHelper::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  if (DlpContentManager::Get()->IsURLConfidential(
          render_frame_host->GetLastCommittedURL())) {
    const bool inserted = confidential_frames_.insert(render_frame_host).second;
    if (inserted && confidential_frames_.size() == 1) {
      DlpContentManager::Get()->OnConfidentialityChanged(web_contents(),
                                                         /*confidential=*/true);
    }
  }
}

void DlpContentTabHelper::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  const bool erased = confidential_frames_.erase(render_frame_host);
  if (erased && confidential_frames_.empty())
    DlpContentManager::Get()->OnConfidentialityChanged(web_contents(),
                                                       /*confidential=*/false);
}

void DlpContentTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted() || navigation_handle->IsErrorPage())
    return;
  if (DlpContentManager::Get()->IsURLConfidential(
          navigation_handle->GetURL())) {
    const bool inserted =
        confidential_frames_.insert(navigation_handle->GetRenderFrameHost())
            .second;
    if (inserted && confidential_frames_.size() == 1) {
      DlpContentManager::Get()->OnConfidentialityChanged(web_contents(),
                                                         /*confidential=*/true);
    }
  } else {
    const bool erased =
        confidential_frames_.erase(navigation_handle->GetRenderFrameHost());
    if (erased && confidential_frames_.empty())
      DlpContentManager::Get()->OnConfidentialityChanged(
          web_contents(),
          /*confidential=*/false);
  }
}

void DlpContentTabHelper::WebContentsDestroyed() {
  DlpContentManager::Get()->OnWebContentsDestroyed(web_contents());
}

void DlpContentTabHelper::OnVisibilityChanged(content::Visibility visibility) {
  // DlpContentManager tracks visibility only for confidential WebContents.
  if (!IsConfidential())
    return;
  DlpContentManager::Get()->OnVisibilityChanged(
      web_contents(), visibility == content::Visibility::VISIBLE);
}

DlpContentTabHelper::DlpContentTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

bool DlpContentTabHelper::IsConfidential() const {
  return !confidential_frames_.empty();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(DlpContentTabHelper)

}  // namespace policy
