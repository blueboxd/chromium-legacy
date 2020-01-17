// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_CRW_PROPERTIES_STORE_H_
#define IOS_WEB_WEB_STATE_UI_CRW_PROPERTIES_STORE_H_

#import <Foundation/Foundation.h>

// An attribute of a property with an Objective-C object type to be preserved.
typedef NS_ENUM(NSInteger, CRWStoredPropertyAttribute) {
  // "strong" attribute.
  CRWStoredPropertyAttributeStrong,
  // "weak" attribute.
  CRWStoredPropertyAttributeWeak,
  // "copy" attribute.
  CRWStoredPropertyAttributeCopy,
};

// An object which preserves properties of an underlying object when the
// underlying object is reassigned.
//
// This is useful when:
//   - A class is a proxy for an underlying object.
//   - The underlying object can be nil or reassigned during the lifetime of the
//     wrapper.
//   - The proxy should preserve a subset of the properties of the underlying
//     object when the underlying object is reassigned.
//
// A caller must call its "register" methods to register properties to be
// preserved before using it.
//
// TODO(crbug.com/1023250): Add a unit test for this class.
// TODO(crbug.com/1023250): Add more safety checks for this class.
@interface CRWPropertiesStore : NSObject

// Registers a property with an Objective-C object type to be preserved.
// |getter| and |setter| are selectors of the getter/setter of the underlying
// object.
- (void)registerObjectPropertyWithGetter:(nonnull SEL)getter
                                  setter:(nonnull SEL)setter
                               attribute:(CRWStoredPropertyAttribute)attribute;

// Registers a property with a type not an Objective-C object to be preserved.
// |getter| and |setter| are selectors of the getter/setter of the underlying
// object. |size| is the byte size of the property type.
//
// This should be used e.g., for scalar types (NSInteger, CGFloat, etc.) and C
// structures (CGRect, CGPoint, etc.).
- (void)registerNonObjectPropertyWithGetter:(nonnull SEL)getter
                                     setter:(nonnull SEL)setter
                                       size:(NSUInteger)size;

// Saves the properties of |object| to the properties store. Should be called
// against the old underlying object when the underlying object is reassigned.
- (void)savePropertiesFromObject:(nonnull id)object;

// Loads the properties from the properties store to |object|. Should be called
// against the new underlying object when the underlying object is reassigned.
- (void)loadPropertiesToObject:(nonnull id)object;

// Clears values of all the properties in the properties store. This prevents
// from retaining property values no longer needed. It does not reset
// registration of properties.
- (void)clearValues;

// Forwards |invocation| to the properties store.
//
// If |invocation| is an invocation of a getter or setter of a registered
// property, it gets or sets the property in the property store, which is later
// restored in the underlying object, and returns YES. Otherwise does nothing
// and returns NO.
//
// This should be called in -forwardInvocation: of the wrapper when the
// underlying object is nil.
- (BOOL)forwardInvocationToPropertiesStore:(nonnull NSInvocation*)invocation;

@end

#endif  // IOS_WEB_WEB_STATE_UI_CRW_PROPERTIES_STORE_H_
