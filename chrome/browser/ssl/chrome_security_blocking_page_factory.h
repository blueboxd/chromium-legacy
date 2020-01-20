// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_CHROME_SECURITY_BLOCKING_PAGE_FACTORY_H_
#define CHROME_BROWSER_SSL_CHROME_SECURITY_BLOCKING_PAGE_FACTORY_H_

#include "base/macros.h"
#include "components/security_interstitials/content/bad_clock_blocking_page.h"
#include "components/security_interstitials/content/blocked_interception_blocking_page.h"
#include "components/security_interstitials/content/captive_portal_blocking_page.h"
#include "components/security_interstitials/content/mitm_software_blocking_page.h"
#include "components/security_interstitials/content/ssl_blocking_page.h"
#include "components/security_interstitials/content/ssl_blocking_page_base.h"

// Contains utilities for Chrome-specific construction of security-related
// interstitial pages.
class ChromeSecurityBlockingPageFactory {
 public:
  // Creates an SSL blocking page. |options_mask| must be a bitwise mask of
  // SSLErrorUI::SSLErrorOptionsMask values. The caller is responsible for
  // ownership of the returned object.
  static SSLBlockingPage* CreateSSLPage(
      content::WebContents* web_contents,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      int options_mask,
      const base::Time& time_triggered,
      const GURL& support_url,
      std::unique_ptr<SSLCertReporter> ssl_cert_reporter);

  // Creates a captive portal blocking page. The caller is responsible for
  // ownership of the returned object.
  static CaptivePortalBlockingPage* CreateCaptivePortalBlockingPage(
      content::WebContents* web_contents,
      const GURL& request_url,
      const GURL& login_url,
      std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
      const net::SSLInfo& ssl_info,
      int cert_error);

  // Creates a bad clock blocking page. The caller is responsible for
  // ownership of the returned object.
  static BadClockBlockingPage* CreateBadClockBlockingPage(
      content::WebContents* web_contents,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      const base::Time& time_triggered,
      ssl_errors::ClockState clock_state,
      std::unique_ptr<SSLCertReporter> ssl_cert_reporter);

  // Creates a man-in-the-middle software blocking page. The caller is
  // responsible for ownership of the returned object.
  static MITMSoftwareBlockingPage* CreateMITMSoftwareBlockingPage(
      content::WebContents* web_contents,
      int cert_error,
      const GURL& request_url,
      std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
      const net::SSLInfo& ssl_info,
      const std::string& mitm_software_name);

  // Creates a blocked interception blocking page. The caller is
  // responsible for ownership of the returned object.
  static BlockedInterceptionBlockingPage* CreateBlockedInterceptionBlockingPage(
      content::WebContents* web_contents,
      int cert_error,
      const GURL& request_url,
      std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
      const net::SSLInfo& ssl_info);

  // Does setup on |page| that is specific to the client (Chrome).
  static void DoChromeSpecificSetup(SSLBlockingPageBase* page);

  // Overrides the calculation of whether the app is enterprise-managed for
  // tests.
  static void SetEnterpriseManagedForTesting(bool enterprise_managed);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ChromeSecurityBlockingPageFactory);
};

#endif  // CHROME_BROWSER_SSL_CHROME_SECURITY_BLOCKING_PAGE_FACTORY_H_
