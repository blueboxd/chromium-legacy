// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ssl/ios_ssl_error_tab_helper.h"

#include "base/values.h"
#include "ios/chrome/browser/interstitials/ios_security_interstitial_page.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_user_data.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Script command prefix.
const char kCommandPrefix[] = "blockingPage";
}  // namespace

IOSSSLErrorTabHelper::IOSSSLErrorTabHelper(web::WebState* web_state)
    : web_state_(web_state), subscription_(nullptr), weak_factory_(this) {
  web_state_->AddObserver(this);
  auto command_callback = base::Bind(
      &IOSSSLErrorTabHelper::OnBlockingPageCommand, weak_factory_.GetWeakPtr());
  subscription_ =
      web_state->AddScriptCommandCallback(command_callback, kCommandPrefix);
}

IOSSSLErrorTabHelper::~IOSSSLErrorTabHelper() = default;

// static
void IOSSSLErrorTabHelper::AssociateBlockingPage(
    web::WebState* web_state,
    int64_t navigation_id,
    std::unique_ptr<IOSSecurityInterstitialPage> blocking_page) {
  // CreateForWebState() creates a tab helper if it doesn't exist for
  // |web_state| yet.
  IOSSSLErrorTabHelper::CreateForWebState(web_state);

  IOSSSLErrorTabHelper* helper = IOSSSLErrorTabHelper::FromWebState(web_state);
  helper->SetBlockingPage(navigation_id, std::move(blocking_page));
}

// When the navigation finishes and commits the SSL error page, store the
// IOSSecurityInterstitialPage in a member variable so that it can handle
// commands. Clean up the member variable when a subsequent navigation commits,
// since the IOSSecurityInterstitialPage is no longer needed.
void IOSSSLErrorTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  DCHECK_EQ(web_state_, web_state);
  if (navigation_context->IsSameDocument()) {
    return;
  }

  auto it = blocking_pages_for_navigations_.find(
      navigation_context->GetNavigationId());

  if (navigation_context->HasCommitted()) {
    if (it == blocking_pages_for_navigations_.end()) {
      blocking_page_for_currently_committed_navigation_.reset();
    } else {
      blocking_page_for_currently_committed_navigation_ = std::move(it->second);
    }
  }

  if (it != blocking_pages_for_navigations_.end()) {
    blocking_pages_for_navigations_.erase(it);
  }

  // Interstitials may change the visibility of the URL or other security state.
  web_state_->DidChangeVisibleSecurityState();
}

void IOSSSLErrorTabHelper::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
}

void IOSSSLErrorTabHelper::SetBlockingPage(
    int64_t navigation_id,
    std::unique_ptr<IOSSecurityInterstitialPage> blocking_page) {
  blocking_pages_for_navigations_[navigation_id] = std::move(blocking_page);
}

void IOSSSLErrorTabHelper::OnBlockingPageCommand(
    const base::DictionaryValue& message,
    const GURL& url,
    bool user_is_interacting,
    web::WebFrame* sender_frame) {
  std::string command;
  if (!message.GetString("command", &command)) {
    DLOG(WARNING) << "JS message parameter not found: command";
  } else {
    if (blocking_page_for_currently_committed_navigation_) {
      blocking_page_for_currently_committed_navigation_->HandleScriptCommand(
          message, url, user_is_interacting, sender_frame);
    }
  }
}

WEB_STATE_USER_DATA_KEY_IMPL(IOSSSLErrorTabHelper)
