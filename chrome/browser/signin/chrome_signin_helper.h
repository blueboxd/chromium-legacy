// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_HELPER_H_
#define CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_HELPER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/supports_user_data.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "content/public/browser/resource_request_info.h"

namespace content_settings {
class CookieSettings;
}

namespace net {
class HttpResponseHeaders;
class URLRequest;
}

class GURL;

// Utility functions for handling Chrome/Gaia headers during signin process.
// Chrome identity should always stay in sync with Gaia identity. Therefore
// Chrome needs to send Gaia special header for requests from a connected
// profile, so that Gaia can modify its response accordingly and let Chrome
// handle signin accordingly.
namespace signin {

// Key for ManageAccountsHeaderReceivedUserData. Exposed for testing.
extern const void* const kManageAccountsHeaderReceivedUserDataKey;

class ChromeRequestAdapter : public RequestAdapter {
 public:
  explicit ChromeRequestAdapter(net::URLRequest* request);
  ~ChromeRequestAdapter() override;

  virtual content::ResourceRequestInfo::WebContentsGetter GetWebContentsGetter()
      const;
  virtual content::ResourceType GetResourceType() const;
  virtual GURL GetReferrerOrigin() const;

  // Associate a callback with this request which will be executed when the
  // request is complete (including any redirects). If a callback was already
  // registered this function does nothing.
  virtual void SetDestructionCallback(base::OnceClosure closure);

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeRequestAdapter);
};

class ResponseAdapter {
 public:
  explicit ResponseAdapter(net::URLRequest* request);
  virtual ~ResponseAdapter();

  virtual content::ResourceRequestInfo::WebContentsGetter GetWebContentsGetter()
      const;
  virtual bool IsMainFrame() const;
  virtual GURL GetOrigin() const;
  virtual const net::HttpResponseHeaders* GetHeaders() const;
  virtual void RemoveHeader(const std::string& name);

  virtual base::SupportsUserData::Data* GetUserData(const void* key) const;
  virtual void SetUserData(const void* key,
                           std::unique_ptr<base::SupportsUserData::Data> data);

 private:
  net::URLRequest* const request_;

  DISALLOW_COPY_AND_ASSIGN(ResponseAdapter);
};

// When Dice is enabled, the AccountReconcilor is blocked for a short delay
// after sending requests to Gaia. Exposed for testing.
void SetDiceAccountReconcilorBlockDelayForTesting(int delay_ms);

// Adds an account consistency header to Gaia requests from a connected profile,
// with the exception of requests from gaia webview.
// Returns true if the account consistency header was added to the request.
// Removes the header if it is already in the headers but should not be there.
void FixAccountConsistencyRequestHeader(
    ChromeRequestAdapter* request,
    const GURL& redirect_url,
    bool is_off_the_record,
    int incognito_availibility,
    AccountConsistencyMethod account_consistency,
    std::string primary_account_id,
#if defined(OS_CHROMEOS)
    bool account_consistency_mirror_required,
#endif
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    bool is_sync_enabled,
    std::string signin_scoped_device_id,
#endif
    content_settings::CookieSettings* cookie_settings);

// Processes account consistency response headers (X-Chrome-Manage-Accounts and
// Dice). |redirect_url| is empty if the request is not a redirect.
void ProcessAccountConsistencyResponseHeaders(ResponseAdapter* response,
                                              const GURL& redirect_url,
                                              bool is_off_the_record);

}  // namespace signin

#endif  // CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_HELPER_H_
