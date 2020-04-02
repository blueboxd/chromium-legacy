// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/page_info/page_info_site_security_mediator.h"

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/security_state/core/security_state.h"
#include "components/ssl_errors/error_info.h"
#include "components/strings/grit/components_chromium_strings.h"
#include "components/strings/grit/components_google_chrome_strings.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/ui/page_info/page_info_site_security_description.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/components/webui/web_ui_url_constants.h"
#include "ios/web/public/security/ssl_status.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Build the certificate details based on the |SSLStatus| and the |URL|.
NSString* BuildCertificateDetailString(web::SSLStatus& SSLStatus,
                                       const GURL& URL) {
  NSMutableString* certificateDetails = [NSMutableString
      stringWithString:l10n_util::GetNSString(
                           IDS_PAGE_INFO_SECURITY_TAB_INSECURE_IDENTITY)];
  NSString* bullet = @"\n • ";
  std::vector<ssl_errors::ErrorInfo> errors;
  ssl_errors::ErrorInfo::GetErrorsForCertStatus(
      SSLStatus.certificate, SSLStatus.cert_status, URL, &errors);
  for (size_t i = 0; i < errors.size(); ++i) {
    [certificateDetails appendString:bullet];
    [certificateDetails
        appendString:base::SysUTF16ToNSString(errors[i].short_description())];
  }

  return certificateDetails;
}

// Returns a messages, based on the |messagesComponents|, joined by a spacing.
NSString* BuildMessage(NSArray<NSString*>* messageComponents) {
  DCHECK(messageComponents.count > 0);
  NSMutableString* message =
      [NSMutableString stringWithString:messageComponents[0]];
  for (NSUInteger index = 1; index < messageComponents.count; index++) {
    NSString* component = messageComponents[index];
    if (component.length == 0)
      continue;
    [message appendString:@"\n\n"];
    [message appendString:component];
  }
  return message;
}

}  // namespace

@implementation PageInfoSiteSecurityMediator

+ (PageInfoSiteSecurityDescription*)configurationForURL:(const GURL&)URL
                                              SSLStatus:(web::SSLStatus&)status
                                            offlinePage:(BOOL)offlinePage {
  PageInfoSiteSecurityDescription* dataHolder =
      [[PageInfoSiteSecurityDescription alloc] init];
  if (offlinePage) {
    dataHolder.title = l10n_util::GetNSString(IDS_IOS_PAGE_INFO_OFFLINE_TITLE);
    dataHolder.message = l10n_util::GetNSString(IDS_IOS_PAGE_INFO_OFFLINE_PAGE);
    dataHolder.image = [UIImage imageNamed:@"page_info_offline"];
    dataHolder.buttonAction = PageInfoSiteSecurityButtonActionReload;
    return dataHolder;
  }

  if (URL.SchemeIs(kChromeUIScheme)) {
    dataHolder.title = base::SysUTF8ToNSString(URL.spec());
    dataHolder.message = l10n_util::GetNSString(IDS_PAGE_INFO_INTERNAL_PAGE);
    dataHolder.image = nil;
    dataHolder.buttonAction = PageInfoSiteSecurityButtonActionNone;
    return dataHolder;
  }

  // At this point, this is a web page.
  dataHolder.title = base::SysUTF8ToNSString(URL.host());
  dataHolder.buttonAction = PageInfoSiteSecurityButtonActionShowHelp;

  // Summary and details.
  if (!status.certificate) {
    // Not HTTPS. This maps to the WARNING security level. Show the grey
    // triangle icon in page info based on the same logic used to determine
    // the iconography in the omnibox.
    if (security_state::ShouldShowDangerTriangleForWarningLevel()) {
      dataHolder.image = [UIImage imageNamed:@"page_info_bad"];
    } else {
      dataHolder.image = [UIImage imageNamed:@"page_info_info"];
    }
    dataHolder.message = BuildMessage(@[
      l10n_util::GetNSString(IDS_PAGE_INFO_NOT_SECURE_SUMMARY),
      l10n_util::GetNSString(IDS_PAGE_INFO_NOT_SECURE_DETAILS)
    ]);
    return dataHolder;
  }

  // It is possible to have |SECURITY_STYLE_AUTHENTICATION_BROKEN| and non-error
  // |cert_status| for WKWebView because |security_style| and |cert_status|
  // are
  // calculated using different API, which may lead to different cert
  // verification results.
  if (net::IsCertStatusError(status.cert_status) ||
      status.security_style == web::SECURITY_STYLE_AUTHENTICATION_BROKEN) {
    // HTTPS with major errors
    dataHolder.image = [UIImage imageNamed:@"page_info_bad"];

    NSString* certificateDetails = BuildCertificateDetailString(status, URL);

    dataHolder.message = BuildMessage(@[
      l10n_util::GetNSString(IDS_PAGE_INFO_NOT_SECURE_SUMMARY),
      l10n_util::GetNSString(IDS_PAGE_INFO_NOT_SECURE_DETAILS),
      certificateDetails
    ]);

    return dataHolder;
  }

  // The remaining states are valid HTTPS, or HTTPS with minor errors.

  base::string16 issuerName(
      base::UTF8ToUTF16(status.certificate->issuer().GetDisplayName()));
  // Have certificateDetails be an empty string to help building the message.
  NSString* certificateDetails = @"";
  if (!issuerName.empty()) {
    // Show the issuer name if it's available.
    // TODO(crbug.com/502470): Implement a certificate viewer instead.
    certificateDetails = l10n_util::GetNSStringF(
        IDS_IOS_PAGE_INFO_SECURITY_TAB_SECURE_IDENTITY, issuerName);
  }

  if (status.content_status == web::SSLStatus::DISPLAYED_INSECURE_CONTENT) {
    // HTTPS with mixed content. This maps to the WARNING security level in M80,
    // so assume the WARNING state when determining whether to swap the icon for
    // a grey triangle. This will result in an inconsistency between the omnibox
    // and page info if the mixed content WARNING feature is disabled.
    if (security_state::ShouldShowDangerTriangleForWarningLevel()) {
      dataHolder.image = [UIImage imageNamed:@"page_info_bad"];
    } else {
      dataHolder.image = [UIImage imageNamed:@"page_info_info"];
    }
    dataHolder.message = BuildMessage(@[
      l10n_util::GetNSString(IDS_PAGE_INFO_MIXED_CONTENT_SUMMARY),
      l10n_util::GetNSString(IDS_PAGE_INFO_MIXED_CONTENT_DETAILS),
      certificateDetails
    ]);

    return dataHolder;
  }

  // Valid HTTPS
  dataHolder.image = [UIImage imageNamed:@"page_info_good"];
  dataHolder.message = BuildMessage(@[
    l10n_util::GetNSString(IDS_PAGE_INFO_SECURE_SUMMARY),
    l10n_util::GetNSString(IDS_PAGE_INFO_SECURE_DETAILS), certificateDetails
  ]);

  DCHECK(!(status.cert_status & net::CERT_STATUS_IS_EV))
      << "Extended Validation should be disabled";

  return dataHolder;
}

@end
