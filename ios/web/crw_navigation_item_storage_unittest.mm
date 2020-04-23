// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/session/crw_navigation_item_storage.h"

#import <Foundation/Foundation.h>
#include <stdint.h>

#include <utility>

#include "base/strings/sys_string_conversions.h"
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/navigation/navigation_item_storage_test_util.h"
#include "ios/web/public/navigation/referrer.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/gtest_support.h"
#include "ui/base/page_transition_types.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class CRWNavigationItemStorageTest : public PlatformTest {
 protected:
  CRWNavigationItemStorageTest()
      : item_storage_([[CRWNavigationItemStorage alloc] init]) {
    // Set up |item_storage_|.
    [item_storage_ setURL:GURL("http://url.test")];
    [item_storage_ setVirtualURL:GURL("http://virtual.test")];
    [item_storage_ setReferrer:web::Referrer(GURL("http://referrer.url"),
                                             web::ReferrerPolicyDefault)];
    [item_storage_ setTimestamp:base::Time::Now()];
    [item_storage_ setTitle:base::SysNSStringToUTF16(@"Title")];
    [item_storage_
        setDisplayState:web::PageDisplayState(CGPointZero, UIEdgeInsetsZero,
                                              0.0, 0.0, 0.0)];
    [item_storage_
        setPOSTData:[@"Test data" dataUsingEncoding:NSUTF8StringEncoding]];
    [item_storage_ setHTTPRequestHeaders:@{@"HeaderKey" : @"HeaderValue"}];
    [item_storage_ setUserAgentType:web::UserAgentType::DESKTOP];
  }

  // Convenience getter to facilitate dot notation in tests.
  CRWNavigationItemStorage* item_storage() { return item_storage_; }

 protected:
  CRWNavigationItemStorage* item_storage_;
};

// Tests that unarchiving CRWNavigationItemStorage data results in an equivalent
// storage.
TEST_F(CRWNavigationItemStorageTest, EncodeDecode) {
  NSData* data = [NSKeyedArchiver archivedDataWithRootObject:item_storage()
                                       requiringSecureCoding:NO
                                                       error:nil];

  NSKeyedUnarchiver* unarchiver =
      [[NSKeyedUnarchiver alloc] initForReadingFromData:data error:nil];
  unarchiver.requiresSecureCoding = NO;
  id decoded = [unarchiver decodeObjectForKey:NSKeyedArchiveRootObjectKey];
  EXPECT_TRUE(web::ItemStoragesAreEqual(item_storage(), decoded));
}

// Tests that unarchiving CRWNavigationItemStorage data with the URL key being
// removed is working.
// TODO(crbug.com/1073378): this is a temporary workaround added in M84 to
// support old client that don't have the kNavigationItemStorageURLKey. It
// should be removed once enough time has passed.
TEST_F(CRWNavigationItemStorageTest, EncodeDecodeNoURL) {
  NSKeyedArchiver* archiver =
      [[NSKeyedArchiver alloc] initRequiringSecureCoding:NO];
  std::string virtualURL = item_storage().virtualURL.spec();
  [archiver encodeBytes:reinterpret_cast<const uint8_t*>(virtualURL.data())
                 length:virtualURL.size()
                 forKey:web::kNavigationItemStorageVirtualURLKey];

  NSKeyedUnarchiver* unarchiver =
      [[NSKeyedUnarchiver alloc] initForReadingFromData:archiver.encodedData
                                                  error:nil];
  unarchiver.requiresSecureCoding = NO;
  CRWNavigationItemStorage* decoded =
      [[CRWNavigationItemStorage alloc] initWithCoder:unarchiver];

  // If the URL isn't encoded, the virtual URL is used.
  EXPECT_EQ(item_storage().virtualURL, decoded.URL);
  EXPECT_EQ(item_storage().virtualURL, decoded.virtualURL);
}
