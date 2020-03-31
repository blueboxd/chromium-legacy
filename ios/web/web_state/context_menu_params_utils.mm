// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/context_menu_params_utils.h"

#include "base/strings/sys_string_conversions.h"
#include "components/url_formatter/url_formatter.h"
#include "ios/web/common/referrer_util.h"
#import "ios/web/web_state/context_menu_constants.h"
#include "ui/gfx/text_elider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

NSString* GetContextMenuTitle(NSDictionary* element) {
  NSString* title = nil;
  NSString* href = element[web::kContextMenuElementHyperlink];
  if (href) {
    GURL link_url = GURL(base::SysNSStringToUTF8(href));
    if (link_url.SchemeIs(url::kJavaScriptScheme)) {
      title = @"JavaScript";
    } else {
      base::string16 URLText = url_formatter::FormatUrl(link_url);
      title = base::SysUTF16ToNSString(URLText);
    }
  }
  NSString* src = element[web::kContextMenuElementSource];
  if (!title)
    title = [src copy];
  if ([title hasPrefix:base::SysUTF8ToNSString(url::kDataScheme)])
    title = nil;
  NSString* title_attribute = element[web::kContextMenuElementTitle];
  if (title_attribute)
    title = title_attribute;
  // Prepend the alt text attribute if element is an image without a link.
  NSString* alt_text = element[web::kContextMenuElementAlt];
  if (alt_text && src && !href)
    title = [NSString stringWithFormat:@"%@ – %@", alt_text, title];
  if ([title length] > web::kContextMenuMaxTitleLength) {
    base::string16 shortened_title;
    gfx::ElideString(base::SysNSStringToUTF16(title),
                     web::kContextMenuMaxTitleLength, &shortened_title);
    title = base::SysUTF16ToNSString(shortened_title);
  }
  return title;
}

}  // namespace

namespace web {

BOOL CanShowContextMenuForElementDictionary(NSDictionary* element) {
  NSString* href = element[kContextMenuElementHyperlink];
  if (GURL(base::SysNSStringToUTF8(href)).is_valid()) {
    return YES;
  }
  NSString* src = element[kContextMenuElementSource];
  if (GURL(base::SysNSStringToUTF8(src)).is_valid()) {
    return YES;
  }
  return NO;
}

ContextMenuParams ContextMenuParamsFromElementDictionary(
    NSDictionary* element) {
  ContextMenuParams params;
  NSString* href = element[kContextMenuElementHyperlink];
  if (href)
    params.link_url = GURL(base::SysNSStringToUTF8(href));
  NSString* src = element[kContextMenuElementSource];
  if (src)
    params.src_url = GURL(base::SysNSStringToUTF8(src));
  NSString* referrer_policy = element[kContextMenuElementReferrerPolicy];
  if (referrer_policy) {
    params.referrer_policy =
        web::ReferrerPolicyFromString(base::SysNSStringToUTF8(referrer_policy));
  }
  NSString* inner_text = element[kContextMenuElementInnerText];
  if ([inner_text length] > 0)
    params.link_text = [inner_text copy];
  params.menu_title = GetContextMenuTitle(element);
  return params;
}

}  // namespace web
