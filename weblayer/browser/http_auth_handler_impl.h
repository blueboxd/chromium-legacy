// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_HTTP_AUTH_HANDLER_IMPL_H_
#define WEBLAYER_BROWSER_HTTP_AUTH_HANDLER_IMPL_H_

#include <memory>
#include <string>

#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/login_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace weblayer {

// Implements support for http auth.
class HttpAuthHandlerImpl : public content::LoginDelegate,
                            public content::WebContentsObserver {
 public:
  HttpAuthHandlerImpl(const net::AuthChallengeInfo& auth_info,
                      content::WebContents* web_contents,
                      bool first_auth_attempt,
                      LoginAuthRequiredCallback callback);
  ~HttpAuthHandlerImpl() override;

  void Proceed(const base::string16& user, const base::string16& password);
  void Cancel();

  GURL url() { return url_; }

 private:
  GURL url_;
  LoginAuthRequiredCallback callback_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_HTTP_AUTH_HANDLER_IMPL_H_