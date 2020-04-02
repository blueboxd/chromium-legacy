// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/page_info/page_info_description.h"

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_chromium_strings.h"
#include "components/strings/grit/components_google_chrome_strings.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/components/webui/web_ui_url_constants.h"
#include "ios/web/public/security/ssl_status.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PageInfoDescription ()

@property(nonatomic, assign) web::SSLStatus status;
@property(nonatomic, assign) const GURL& URL;
@property(nonatomic, assign) BOOL isOffline;

@end

@implementation PageInfoDescription {
  GURL _URL;
}

- (instancetype)initWithURL:(const GURL&)URL
                  SSLStatus:(const web::SSLStatus&)status
              isPageOffline:(BOOL)isOffline {
  self = [super init];
  if (self) {
    _URL = URL;
    _status = status;
    _isOffline = isOffline;
  }
  return self;
}

- (NSString*)pageSecurityStatusDescription {
  if (self.isOffline) {
    return l10n_util::GetNSString(IDS_IOS_PAGE_INFO_OFFLINE_PAGE);
  }

  if (self.URL.SchemeIs(kChromeUIScheme)) {
    return l10n_util::GetNSString(IDS_PAGE_INFO_INTERNAL_PAGE);
  }

  if (!self.status.certificate) {
    return l10n_util::GetNSString(IDS_PAGE_INFO_NOT_SECURE_SUMMARY);
  }

  if (net::IsCertStatusError((self.status.cert_status)) ||
      (self.status.security_style ==
       web::SECURITY_STYLE_AUTHENTICATION_BROKEN)) {
    // HTTPS with major errors
    return l10n_util::GetNSString(IDS_PAGE_INFO_NOT_SECURE_SUMMARY);
  }

  if (self.status.content_status ==
      web::SSLStatus::DISPLAYED_INSECURE_CONTENT) {
    return l10n_util::GetNSString(IDS_PAGE_INFO_NOT_SECURE_SUMMARY);
  }

  // Valid HTTPS
  return l10n_util::GetNSString(IDS_PAGE_INFO_SECURE_SUMMARY);
}

- (NSString*)pageSecurityStatusIconImageName {
  // If the URL scheme corresponds to Chrome on iOS, the icon is not set.
  if (self.URL.SchemeIs(kChromeUIScheme))
    return @"";

  if (self.isOffline)
    return @"page_info_offline";

  if (!self.status.certificate) {
    if (security_state::ShouldShowDangerTriangleForWarningLevel())
      return @"page_info_bad";

    return @"page_info_info";
  }

  if (net::IsCertStatusError(self.status.cert_status) ||
      self.status.security_style == web::SECURITY_STYLE_AUTHENTICATION_BROKEN)
    // HTTPS with major errors
    return @"page_info_bad";

  // The remaining states are valid HTTPS, or HTTPS with minor errors.
  if (self.status.content_status ==
      web::SSLStatus::DISPLAYED_INSECURE_CONTENT) {
    if (security_state::ShouldShowDangerTriangleForWarningLevel())
      return @"page_info_bad";

    return @"page_info";
  }

  // Valid HTTPS
  return @"page_info_good";
}

#pragma mark - Properties

- (const GURL&)URL {
  return _URL;
}

@end
