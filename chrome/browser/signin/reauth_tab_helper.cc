// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/reauth_tab_helper.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/signin/reauth_result.h"
#include "content/public/browser/navigation_handle.h"
#include "url/origin.h"

namespace signin {

// static
void ReauthTabHelper::CreateForWebContents(content::WebContents* web_contents,
                                           const GURL& reauth_url,
                                           bool restrict_to_reauth_origin,
                                           ReauthCallback callback) {
  DCHECK(web_contents);
  if (!FromWebContents(web_contents)) {
    web_contents->SetUserData(
        UserDataKey(), base::WrapUnique(new ReauthTabHelper(
                           web_contents, reauth_url, restrict_to_reauth_origin,
                           std::move(callback))));
  } else {
    std::move(callback).Run(signin::ReauthResult::kCancelled);
  }
}

ReauthTabHelper::~ReauthTabHelper() = default;

bool ReauthTabHelper::ShouldAllowNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame())
    return true;

  if (!restrict_to_reauth_origin_)
    return true;

  return url::IsSameOriginWith(reauth_url_, navigation_handle->GetURL());
}

void ReauthTabHelper::CompleteReauth(signin::ReauthResult result) {
  if (callback_)
    std::move(callback_).Run(result);
}

void ReauthTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame())
    return;

  if (navigation_handle->IsErrorPage()) {
    CompleteReauth(signin::ReauthResult::kLoadFailed);
    return;
  }

  GURL::Replacements replacements;
  replacements.ClearQuery();
  GURL url_without_query =
      navigation_handle->GetURL().ReplaceComponents(replacements);
  if (url_without_query == reauth_url_) {
    // TODO(https://crbug.com/1045515): check for the HTTP_OK response code once
    // Gaia implements a landing page.
    CompleteReauth(signin::ReauthResult::kSuccess);
    return;
  }
}

void ReauthTabHelper::WebContentsDestroyed() {
  CompleteReauth(signin::ReauthResult::kDismissedByUser);
}

ReauthTabHelper::ReauthTabHelper(content::WebContents* web_contents,
                                 const GURL& reauth_url,
                                 bool restrict_to_reauth_origin,
                                 ReauthCallback callback)
    : content::WebContentsObserver(web_contents),
      reauth_url_(reauth_url),
      restrict_to_reauth_origin_(restrict_to_reauth_origin),
      callback_(std::move(callback)) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ReauthTabHelper)

}  // namespace signin
