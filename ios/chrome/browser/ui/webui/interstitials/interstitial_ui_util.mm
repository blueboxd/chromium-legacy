// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/webui/interstitials/interstitial_ui_util.h"

#include "base/atomic_sequence_num.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/time/time.h"
#include "components/grit/dev_ui_components_resources.h"
#include "components/security_interstitials/core/ssl_error_options_mask.h"
#include "crypto/rsa_private_key.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/ssl/ios_captive_portal_blocking_page.h"
#include "ios/chrome/browser/ssl/ios_ssl_blocking_page.h"
#import "ios/chrome/browser/ui/webui/interstitials/interstitial_ui_constants.h"
#import "ios/chrome/browser/ui/webui/interstitials/interstitial_ui_util.h"
#import "ios/web/public/security/web_interstitial_delegate.h"
#include "ios/web/public/webui/url_data_source_ios.h"
#include "ios/web/public/webui/web_ui_ios.h"
#include "ios/web/public/webui/web_ui_ios_data_source.h"
#include "net/base/url_util.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

scoped_refptr<net::X509Certificate> CreateFakeCert() {
  // NSS requires that serial numbers be unique even for the same issuer;
  // as all fake certificates will contain the same issuer name, it's
  // necessary to ensure the serial number is unique, as otherwise
  // NSS will fail to parse.
  static base::AtomicSequenceNumber serial_number;

  std::unique_ptr<crypto::RSAPrivateKey> unused_key;
  std::string cert_der;
  if (!net::x509_util::CreateKeyAndSelfSignedCert(
          "CN=Error", static_cast<uint32_t>(serial_number.GetNext()),
          base::Time::Now() - base::TimeDelta::FromMinutes(5),
          base::Time::Now() + base::TimeDelta::FromMinutes(5), &unused_key,
          &cert_der)) {
    return nullptr;
  }

  return net::X509Certificate::CreateFromBytes(cert_der.data(),
                                               cert_der.size());
}

}

std::unique_ptr<web::WebInterstitialDelegate> CreateSslBlockingPageDelegate(
    web::WebState* web_state,
    const GURL& url) {
  DCHECK_EQ(kChromeInterstitialSslPath, url.path());
  // Fake parameters for SSL blocking page.
  GURL request_url("https://example.com");
  std::string url_param;
  if (net::GetValueForKeyInQuery(url, kChromeInterstitialSslUrlQueryKey,
                                 &url_param)) {
    GURL query_url_param(url_param);
    if (query_url_param.is_valid())
      request_url = query_url_param;
  }

  bool overridable = false;
  std::string overridable_param;
  if (net::GetValueForKeyInQuery(url, kChromeInterstitialSslOverridableQueryKey,
                                 &overridable_param)) {
    overridable = overridable_param == "1";
  }

  bool strict_enforcement = false;
  std::string strict_enforcement_param;
  if (net::GetValueForKeyInQuery(
          url, kChromeInterstitialSslStrictEnforcementQueryKey,
          &strict_enforcement_param)) {
    strict_enforcement = strict_enforcement_param == "1";
  }

  int cert_error = net::ERR_CERT_CONTAINS_ERRORS;
  std::string type_param;
  if (net::GetValueForKeyInQuery(url, kChromeInterstitialSslTypeQueryKey,
                                 &type_param)) {
    if (type_param == kChromeInterstitialSslTypeHpkpFailureQueryValue) {
      cert_error = net::ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN;
    } else if (type_param == kChromeInterstitialSslTypeCtFailureQueryValue) {
      cert_error = net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED;
    }
  }

  net::SSLInfo ssl_info;
  ssl_info.cert = ssl_info.unverified_cert = CreateFakeCert();

  int options_mask = 0;
  if (overridable) {
    options_mask |=
        security_interstitials::SSLErrorOptionsMask::SOFT_OVERRIDE_ENABLED;
  }
  if (strict_enforcement) {
    options_mask |=
        security_interstitials::SSLErrorOptionsMask::STRICT_ENFORCEMENT;
  }

  return std::make_unique<IOSSSLBlockingPage>(
      web_state, cert_error, ssl_info, request_url, options_mask,
      base::Time::NowFromSystemTime(), base::OnceCallback<void(bool)>());
}

std::unique_ptr<web::WebInterstitialDelegate>
CreateCaptivePortalBlockingPageDelegate(web::WebState* web_state) {
  GURL landing_url("https://captive.portal/login");
  GURL request_url("https://google.com");
  return std::make_unique<IOSCaptivePortalBlockingPage>(
      web_state, request_url, landing_url, base::OnceCallback<void(bool)>());
}
