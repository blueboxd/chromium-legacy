// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/earl_grey/web_app_interface.h"

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/web/public/web_client.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation WebAppInterface

+ (NSArray<NSString*>*)additionalStandardSchemes {
  web::WebClient::Schemes webClientSchemes;
  web::GetWebClient()->AddAdditionalSchemes(&webClientSchemes);
  NSMutableArray<NSString*>* schemes = [NSMutableArray<NSString*> array];
  const std::vector<std::string>& standardSchemes =
      webClientSchemes.standard_schemes;
  for (size_t i = 0; i < standardSchemes.size(); ++i) {
    [schemes addObject:base::SysUTF8ToNSString(standardSchemes[i])];
  }
  return schemes;
}

+ (NSArray<NSString*>*)additionalSecureSchemes {
  web::WebClient::Schemes webClientSchemes;
  web::GetWebClient()->AddAdditionalSchemes(&webClientSchemes);
  NSMutableArray<NSString*>* schemes = [NSMutableArray<NSString*> array];
  const std::vector<std::string>& secureSchemes =
      webClientSchemes.secure_schemes;
  for (size_t i = 0; i < secureSchemes.size(); ++i) {
    [schemes addObject:base::SysUTF8ToNSString(secureSchemes[i])];
  }
  return schemes;
}

@end
