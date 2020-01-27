// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_properties_store.h"

#import <Foundation/Foundation.h>

#include "base/compiler_specific.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// A protocol with properties used for the test.
@protocol CRWTestProtocol <NSObject>

@property(nonatomic) NSInteger nonObjectProperty;
@property(nonatomic, strong) NSObject* strongProperty;
@property(nonatomic, weak) NSObject* weakProperty;
@property(nonatomic, copy) NSObject* copiedProperty;

@end

// An object with properties used for the test.
@interface CRWTestObject : NSObject <CRWTestProtocol>
@end

@implementation CRWTestObject

@synthesize nonObjectProperty = _nonObjectProperty;
@synthesize strongProperty = _strongProperty;
@synthesize weakProperty = _weakProperty;
@synthesize copiedProperty = _copiedProperty;

@end

// An object which forwards all invocations to a CRWPropertiesStore.
@interface CRWTestProxy : NSObject

@property(nonatomic) CRWPropertiesStore* propertiesStore;

@end

// Conform to the protocol in a category, which allows to conform to a protocol
// without implementing the properties explicitly.
@interface CRWTestProxy (CRWTestProtocol) <CRWTestProtocol>
@end

@implementation CRWTestProxy

- (NSMethodSignature*)methodSignatureForSelector:(SEL)sel {
  return [CRWTestObject instanceMethodSignatureForSelector:sel];
}

- (void)forwardInvocation:(NSInvocation*)invocation {
  [_propertiesStore forwardInvocationToPropertiesStore:invocation];
}

@end

class CRWPropertiesStoreTest : public PlatformTest {
 public:
  CRWPropertiesStoreTest()
      : properties_store_([[CRWPropertiesStore alloc] init]),
        proxy_([[CRWTestProxy alloc] init]) {
    [properties_store_
        registerNonObjectPropertyWithGetter:@selector(nonObjectProperty)
                                     setter:@selector(setNonObjectProperty:)
                                       type:@encode(NSInteger)];
    [properties_store_
        registerObjectPropertyWithGetter:@selector(strongProperty)
                                  setter:@selector(setStrongProperty:)
                               attribute:CRWStoredPropertyAttributeStrong];
    [properties_store_
        registerObjectPropertyWithGetter:@selector(weakProperty)
                                  setter:@selector(setWeakProperty:)
                               attribute:CRWStoredPropertyAttributeWeak];
    [properties_store_
        registerObjectPropertyWithGetter:@selector(copiedProperty)
                                  setter:@selector(setCopiedProperty:)
                               attribute:CRWStoredPropertyAttributeCopy];

    proxy_.propertiesStore = properties_store_;
  }

 protected:
  CRWPropertiesStore* properties_store_;
  CRWTestProxy* proxy_;
};

// Tests that a value of an non-object type property is preserved when accessed
// via -forwardInvocationToPropertiesStore:.
TEST_F(CRWPropertiesStoreTest, PreserveForwardedNonObjectProperty) {
  EXPECT_EQ(0, proxy_.nonObjectProperty);
  proxy_.nonObjectProperty = 1;
  EXPECT_EQ(1, proxy_.nonObjectProperty);
}

// Tests that a value of a strong property is preserved when accessed via
// -forwardInvocationToPropertiesStore:.
TEST_F(CRWPropertiesStoreTest, PreserveForwardedStrongProperty) {
  NSObject* strong_value = [[NSObject alloc] init];
  __weak NSObject* weak_value = strong_value;

  EXPECT_FALSE(proxy_.strongProperty);
  proxy_.strongProperty = strong_value;
  EXPECT_EQ(strong_value, proxy_.strongProperty);

  // Verify that the object is retained by CRWPropertiesStore.
  strong_value = nil;
  EXPECT_EQ(weak_value, proxy_.strongProperty);
}

// Tests that a value of a weak property is preserved when accessed via
// -forwardInvocationToPropertiesStore:.
TEST_F(CRWPropertiesStoreTest, PreserveForwardedWeakProperty) {
  NSObject* strong_value = [[NSObject alloc] init];

  EXPECT_FALSE(proxy_.weakProperty);
  proxy_.weakProperty = strong_value;
  EXPECT_EQ(strong_value, proxy_.weakProperty);

  // Verify that the object is NOT retained by CRWPropertiesStore.
  strong_value = nil;
  EXPECT_FALSE(proxy_.weakProperty);
}

// Tests that a value of a copy property is preserved when accessed via
// -forwardInvocationToPropertiesStore:.
TEST_F(CRWPropertiesStoreTest, PreserveForwardedCopiedProperty) {
  // Use an NSMutableString as an example object here so that its -copy returns
  // a different object from itself. -[NSString copy] may return itself.
  NSMutableString* string = [@"hoge" mutableCopy];

  EXPECT_FALSE(proxy_.copiedProperty);
  proxy_.copiedProperty = string;

  // Verify that it is copied i.e., it -isEqual: to the original object but not
  // the same instance.
  EXPECT_NSEQ(string, proxy_.copiedProperty);
  EXPECT_NE(string, proxy_.copiedProperty);
}

// Tests that -savePropertiesFromObject: works as expected.
TEST_F(CRWPropertiesStoreTest, SaveProperties) {
  NSObject* object_value1 = [[NSObject alloc] init];
  NSObject* object_value2 = [[NSObject alloc] init];
  NSMutableString* copyable_value = [@"hoge" mutableCopy];

  CRWTestObject* object = [[CRWTestObject alloc] init];
  object.nonObjectProperty = 1;
  object.strongProperty = object_value1;
  object.weakProperty = object_value2;
  object.copiedProperty = copyable_value;

  [properties_store_ savePropertiesFromObject:object];

  EXPECT_EQ(1, proxy_.nonObjectProperty);
  EXPECT_EQ(object_value1, proxy_.strongProperty);
  EXPECT_EQ(object_value2, proxy_.weakProperty);
  EXPECT_NSEQ(copyable_value, proxy_.copiedProperty);
}

// Tests that -loadPropertiesToObject: works as expected.
TEST_F(CRWPropertiesStoreTest, LoadProperties) {
  NSObject* object_value1 = [[NSObject alloc] init];
  NSObject* object_value2 = [[NSObject alloc] init];
  NSMutableString* copyable_value = [@"hoge" mutableCopy];

  proxy_.nonObjectProperty = 1;
  proxy_.strongProperty = object_value1;
  proxy_.weakProperty = object_value2;
  proxy_.copiedProperty = copyable_value;

  CRWTestObject* object = [[CRWTestObject alloc] init];
  [properties_store_ loadPropertiesToObject:object];

  EXPECT_EQ(1, object.nonObjectProperty);
  EXPECT_EQ(object_value1, object.strongProperty);
  EXPECT_EQ(object_value2, object.weakProperty);
  EXPECT_NSEQ(copyable_value, object.copiedProperty);
}

// Tests that -clearValues works as expected.
TEST_F(CRWPropertiesStoreTest, ClearValues) {
  NSObject* object_value1 = [[NSObject alloc] init];
  NSObject* object_value2 = [[NSObject alloc] init];
  NSMutableString* copyable_value = [@"hoge" mutableCopy];

  proxy_.nonObjectProperty = 1;
  proxy_.strongProperty = object_value1;
  proxy_.weakProperty = object_value2;
  proxy_.copiedProperty = copyable_value;

  [properties_store_ clearValues];

  EXPECT_EQ(0, proxy_.nonObjectProperty);
  EXPECT_FALSE(proxy_.strongProperty);
  EXPECT_FALSE(proxy_.weakProperty);
  EXPECT_FALSE(proxy_.copiedProperty);
}
