// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_EARL_GREY_WEB_APP_INTERFACE_H_
#define IOS_WEB_PUBLIC_TEST_EARL_GREY_WEB_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// WebAppInterface contains the app-side implementation for helpers.
// These helpers are compiled into the app binary and can be called from either
// app or test code.
@interface WebAppInterface : NSObject

// Returns a list of additional standard schemes registered by
// web::RegisterWebSchemes().
+ (NSArray<NSString*>*)additionalStandardSchemes;

// Returns a list of additional secure schemes registered by
// web::RegisterWebSchemes().
+ (NSArray<NSString*>*)additionalSecureSchemes;

@end

#endif  // IOS_WEB_PUBLIC_TEST_EARL_GREY_WEB_APP_INTERFACE_H_
