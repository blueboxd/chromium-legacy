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

  // Do not clear the login prompt for subframe or same-document navigations;
  // these could happen in the case of 401/407 error pages that have fancy
  // response bodies that have subframes or can trigger same-document
  // navigations.
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument())
    return;

  if (!delegate_)
    return;

  // TODO(https://crbug.com/943610): this is a very hacky special-case for a
  // particular issue that arises when the server sends an empty body for the
  // 401 or 407 response. In this case, the renderer commits an error page for
  // the HTTP status code (see
  // ChromeContentRendererClient::PrepareErrorPageForHttpStatusError). Error
  // pages are a second commit from the browser's perspective, which runs
  // DidStartNavigation again and therefore would dismiss the prompt if this
  // special case weren't here. To make matters worse, at the error page's
  // second commit, the browser process does not have a NavigationRequest
  // available (see |is_commit_allowed_to_proceed| in
  // RenderFrameHostImpl::DidCommitNavigationInternal), and therefore no
  // AuthChallengeInfo to use to re-show the prompt after it has been
  // dismissed. This whole mess should be fixed in https://crbug.com/943610,
  // which is about enforcing that the browser always has a NavigationRequest
  // available at commit time; once error pages no longer have second commits
  // for which NavigationRequests are manufactured, this special case will no
  // longer be needed.
  //
  // For now, we can hack around it by preserving the login prompt when starting
  // a navigation to the same URL for which we are currently showing a login
  // prompt. There is a possibility that the starting navigation is actually due
  // to the user refreshing rather than the error page committing, and when the
  // user refreshes the server might no longer serve an auth challenge. But we
  // can't distinguish the two scenarios (error page committing vs user
  // refreshing) at this point, so we clear the prompt in DidFinishNavigation if
  // it's not an error page. This behavior could lead to a slight oddness where
  // the prompt lingers around for a bit too long, but this should only happen
  // in the perfect storm where a server's auth response has an empty body,
  // the user refreshes when the prompt is showing, and the server no longer
  // requires auth on the refresh.
  if (navigation_handle->GetURL() == url_for_delegate_)
    return;

  delegate_.reset();
  url_for_delegate_ = GURL();
}

void LoginTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK(
      base::FeatureList::IsEnabled(features::kHTTPAuthCommittedInterstitials));

  // See TODO(https://crbug.com/943610) in DidStartNavigation().
  if (delegate_ && !navigation_handle->IsErrorPage()) {
    delegate_.reset();
    url_for_delegate_ = GURL();
  }

  if (!navigation_handle->GetAuthChallengeInfo()) {
    return;
  }

  // TODO(https://crbug.com/969097): handle auth challenges that lead to
  // downloads (i.e. don't commit).

  // Show a login prompt with the navigation's AuthChallengeInfo on FTP
  // navigations and on HTTP 401/407 responses.
  if (!navigation_handle->GetURL().SchemeIs(url::kFtpScheme)) {
    int response_code =
        navigation_handle->GetResponseHeaders()->response_code();
    if (response_code !=
            net::HttpStatusCode::HTTP_PROXY_AUTHENTICATION_REQUIRED &&
        response_code != net::HttpStatusCode::HTTP_UNAUTHORIZED) {
      return;
    }
  }

  challenge_ = navigation_handle->GetAuthChallengeInfo().value();

  url_for_delegate_ = navigation_handle->GetURL();
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

  // If the challenge comes from a proxy, the URL should be hidden in the
  // omnibox to avoid origin confusion. Call DidChangeVisibleSecurityState() to
  // trigger the omnibox to update, picking up the result of ShouldDisplayURL().
  if (challenge_.is_proxy) {
    navigation_handle->GetWebContents()->DidChangeVisibleSecurityState();
  }
}

bool LoginTabHelper::ShouldDisplayURL() const {
  return !delegate_ || !challenge_.is_proxy;
}

bool LoginTabHelper::IsShowingPrompt() const {
  return !!delegate_;
}

LoginTabHelper::LoginTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

void LoginTabHelper::HandleCredentials(
    const base::Optional<net::AuthCredentials>& credentials) {
  delegate_.reset();
  url_for_delegate_ = GURL();

  if (credentials.has_value()) {
    // Pass a weak pointer for the callback, as the WebContents (and thus this
    // LoginTabHelper) could be destroyed while the network service is
    // processing the new cache entry.
    content::BrowserContext::GetDefaultStoragePartition(
        web_contents()->GetBrowserContext())
        ->GetNetworkContext()
        ->AddAuthCacheEntry(challenge_, credentials.value(),
                            base::BindOnce(&LoginTabHelper::Reload,
                                           weak_ptr_factory_.GetWeakPtr()));
  }

  // Once credentials have been provided, in the case of proxy auth where the
  // URL is hidden when the prompt is showing, trigger
  // DidChangeVisibleSecurityState() to re-show the URL now that the prompt is
  // gone.
  if (challenge_.is_proxy) {
    web_contents()->DidChangeVisibleSecurityState();
  }
}

void LoginTabHelper::Reload() {
  web_contents()->GetController().Reload(content::ReloadType::NORMAL, true);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(LoginTabHelper)
