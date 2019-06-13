// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/login/login_tab_helper.h"

#include "base/feature_list.h"
#include "chrome/browser/ui/login/login_handler.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/login_delegate.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_status_code.h"

LoginTabHelper::~LoginTabHelper() {}

void LoginTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // When navigating away, the LoginHandler for the previous navigation (if any)
  // should get cleared.
  delegate_.reset();
}

void LoginTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK(
      base::FeatureList::IsEnabled(features::kHTTPAuthCommittedInterstitials));

  if (!navigation_handle->GetAuthChallengeInfo()) {
    return;
  }

  // TODO(https://crbug.com/969097): handle auth challenges that lead to
  // downloads (i.e. don't commit).

  int response_code = navigation_handle->GetResponseHeaders()->response_code();
  if (response_code !=
          net::HttpStatusCode::HTTP_PROXY_AUTHENTICATION_REQUIRED &&
      response_code != net::HttpStatusCode::HTTP_UNAUTHORIZED) {
    return;
  }

  challenge_ = navigation_handle->GetAuthChallengeInfo().value();

  delegate_ = CreateLoginPrompt(
      navigation_handle->GetAuthChallengeInfo().value(),
      navigation_handle->GetWebContents(),
      navigation_handle->GetGlobalRequestID(), true,
      navigation_handle->GetURL(),
      // TODO(https://crbug.com/968881): response headers can be null because
      // they are only used for passing the request to extensions, and that
      // doesn't happen in POST_COMMIT mode. This API needs to be cleaned up.
      nullptr, LoginHandler::POST_COMMIT,
      base::BindOnce(
          &LoginTabHelper::HandleCredentials,
          // Since the LoginTabHelper owns the |delegate_| that calls this
          // callback, it's safe to use base::Unretained here; the |delegate_|
          // cannot outlive its owning LoginTabHelper.
          base::Unretained(this)));
}

LoginTabHelper::LoginTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents), weak_ptr_factory_(this) {}

void LoginTabHelper::HandleCredentials(
    const base::Optional<net::AuthCredentials>& credentials) {
  if (!credentials.has_value()) {
    delegate_.reset();
    return;
  }
  // Pass a weak pointer for the callback, as the WebContents (and thus this
  // LoginTabHelper) could be destroyed while the network service is processing
  // the new cache entry.
  content::BrowserContext::GetDefaultStoragePartition(
      web_contents()->GetBrowserContext())
      ->GetNetworkContext()
      ->AddAuthCacheEntry(challenge_, credentials.value(),
                          base::BindOnce(&LoginTabHelper::Reload,
                                         weak_ptr_factory_.GetWeakPtr()));
}

void LoginTabHelper::Reload() {
  web_contents()->GetController().Reload(content::ReloadType::NORMAL, true);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(LoginTabHelper)
