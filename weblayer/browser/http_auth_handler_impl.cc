// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/http_auth_handler_impl.h"

#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "net/base/auth.h"
#include "weblayer/browser/tab_impl.h"

namespace weblayer {

HttpAuthHandlerImpl::HttpAuthHandlerImpl(
    const net::AuthChallengeInfo& auth_info,
    content::WebContents* web_contents,
    bool first_auth_attempt,
    LoginAuthRequiredCallback callback)
    : WebContentsObserver(web_contents), callback_(std::move(callback)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  url_ = auth_info.challenger.GetURL().Resolve(auth_info.path);

  auto* tab = TabImpl::FromWebContents(web_contents);
  tab->ShowHttpAuthPrompt(this);
}

HttpAuthHandlerImpl::~HttpAuthHandlerImpl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* tab = TabImpl::FromWebContents(web_contents());
  if (tab)
    tab->CloseHttpAuthPrompt();
}

void HttpAuthHandlerImpl::Proceed(const base::string16& user,
                                  const base::string16& password) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (callback_) {
    std::move(callback_).Run(net::AuthCredentials(user, password));
  }
}

void HttpAuthHandlerImpl::Cancel() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (callback_) {
    std::move(callback_).Run(base::nullopt);
  }
}

}  // namespace weblayer