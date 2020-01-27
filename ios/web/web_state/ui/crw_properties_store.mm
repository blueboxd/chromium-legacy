// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_properties_store.h"

#include "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// An abstract base class of storage of a single property in CRWPropertiesStore.
@interface CRWPropertyStore : NSObject

- (instancetype)initWithGetter:(SEL)getter
                        setter:(SEL)setter NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// The selector of the getter of the property.
@property(nonatomic, readonly) SEL getter;

// The selector of the setter of the property.
@property(nonatomic, readonly) SEL setter;

// The type encoding of the property type e.g., @encode(BOOL).
@property(nonatomic, readonly) const char* type;

// A pointer to the value with the byte size |size|.
@property(nonatomic, readonly) const void* valueBytes;

// The byte size of the value type.
@property(nonatomic, readonly) NSUInteger size;

// YES if the store contains a value.
@property(nonatomic) BOOL hasValue;

// Sets the value from |bytes|. |bytes| must be a buffer with the byte size
// |size|. This method copies the content of |bytes|. |bytes| must
// not be under control of ARC.
- (void)setValueFromBytes:(const void*)bytes;

// Clears the value. |hasValue| becomes NO after this call.
- (void)clearValue;

@end

// Storage of a single property with "strong" attribute in CRWPropertiesStore.
@interface CRWStrongPropertyStore : CRWPropertyStore
@end

// Storage of a single property with "weak" attribute in CRWPropertiesStore.
@interface CRWWeakPropertyStore : CRWPropertyStore
@end

// Storage of a single property with "copy" attribute in CRWPropertiesStore.
@interface CRWCopyPropertyStore : CRWPropertyStore
@end

// Storage of a single property with an non object type in CRWPropertiesStore.
@interface CRWNonObjectPropertyStore : CRWPropertyStore

- (instancetype)initWithGetter:(SEL)getter
                        setter:(SEL)setter
                          type:(const char*)type NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithGetter:(SEL)getter setter:(SEL)setter NS_UNAVAILABLE;

@end

@implementation CRWPropertyStore

@dynamic valueBytes;
@dynamic type;
@dynamic size;

- (instancetype)initWithGetter:(SEL)getter setter:(SEL)setter {
  self = [super init];
  if (self) {
    _getter = getter;
    _setter = setter;
    _hasValue = NO;
  }
  return self;
}

- (void)setValueFromBytes:(const void*)bytes {
  NOTIMPLEMENTED() << "A subclass must implement this.";
}

- (void)clearValue {
  NOTIMPLEMENTED() << "A subclass must implement this.";
}

@end

@implementation CRWStrongPropertyStore {
  id _value;
}

- (const void*)valueBytes {
  return &_value;
}

- (const char*)type {
  return @encode(id);
}

- (NSUInteger)size {
  return sizeof(id);
}

- (void)setValueFromBytes:(const void*)bytes {
  self.hasValue = YES;
  // |bytes| is not under control of ARC, so it should be treated as
  // __unsafe_unretained. ARC automatically retains the object by this
  // assignment to a strong variable |_value|, so manual retention is not
  // necessary.
  _value = *reinterpret_cast<__unsafe_unretained const id*>(bytes);
}

- (void)clearValue {
  self.hasValue = NO;
  _value = nil;
}

@end

@implementation CRWCopyPropertyStore {
  id _value;
}

- (const void*)valueBytes {
  return &_value;
}

- (const char*)type {
  return @encode(id);
}

- (NSUInteger)size {
  return sizeof(id);
}

- (void)setValueFromBytes:(const void*)bytes {
  self.hasValue = YES;
  // |bytes| is not under control of ARC, so it should be treated as
  // __unsafe_unretained.
  _value =
      [*reinterpret_cast<__unsafe_unretained NSObject* const*>(bytes) copy];
}

- (void)clearValue {
  self.hasValue = NO;
  _value = nil;
}

@end

@implementation CRWWeakPropertyStore {
  __weak id _value;
}

- (const void*)valueBytes {
  return &_value;
}

- (const char*)type {
  return @encode(id);
}

- (NSUInteger)size {
  return sizeof(id);
}

- (void)setValueFromBytes:(const void*)bytes {
  self.hasValue = YES;
  // |bytes| is not under control of ARC, so it should be treated as
  // __unsafe_unretained.
  _value = *reinterpret_cast<__unsafe_unretained const id*>(bytes);
}

- (void)clearValue {
  self.hasValue = NO;
  _value = nil;
}

@end

@implementation CRWNonObjectPropertyStore {
  NSMutableData* _valueData;
}

@synthesize type = _type;
@synthesize size = _size;

- (instancetype)initWithGetter:(SEL)getter
                        setter:(SEL)setter
                          type:(const char*)type {
  self = [super initWithGetter:getter setter:setter];
  if (self) {
    DCHECK_NE(strcmp(type, @encode(id)), 0)
        << "CRWNonObjectPropertyStore must not be used for object types";
    _type = type;
    NSGetSizeAndAlignment(type, &_size, nullptr);
    _valueData = [NSMutableData dataWithLength:_size];
  }
  return self;
}

- (const void*)valueBytes {
  return _valueData.bytes;
}

- (void)setValueFromBytes:(const void*)bytes {
  self.hasValue = YES;
  [_valueData replaceBytesInRange:NSMakeRange(0, _valueData.length)
                        withBytes:bytes];
}

- (void)clearValue {
  self.hasValue = NO;
  [_valueData resetBytesInRange:NSMakeRange(0, _valueData.length)];
}

@end

@implementation CRWPropertiesStore {
  // A list of stores for registered properties.
  NSMutableArray<CRWPropertyStore*>* _propertyStores;

  // A dictionary where:
  //   - The key is NSValue for SEL of the getter of the property.
  //   - The value is a store of the property.
  NSMutableDictionary<NSValue*, CRWPropertyStore*>* _propertyStoreByGetter;

  // A dictionary where:
  //   - The key is NSValue for SEL of the setter of the property.
  //   - The value is a store of the property.
  NSMutableDictionary<NSValue*, CRWPropertyStore*>* _propertyStoreBySetter;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _propertyStores = [[NSMutableArray alloc] init];
    _propertyStoreByGetter = [[NSMutableDictionary alloc] init];
    _propertyStoreBySetter = [[NSMutableDictionary alloc] init];
  }
  return self;
}

- (void)registerObjectPropertyWithGetter:(SEL)getter
                                  setter:(SEL)setter
                               attribute:(CRWStoredPropertyAttribute)attribute {
  CRWPropertyStore* propertyStore;
  switch (attribute) {
    case CRWStoredPropertyAttributeStrong:
      propertyStore = [[CRWStrongPropertyStore alloc] initWithGetter:getter
                                                              setter:setter];
      break;
    case CRWStoredPropertyAttributeWeak:
      propertyStore = [[CRWWeakPropertyStore alloc] initWithGetter:getter
                                                            setter:setter];
      break;
    case CRWStoredPropertyAttributeCopy:
      propertyStore = [[CRWCopyPropertyStore alloc] initWithGetter:getter
                                                            setter:setter];
      break;
  }
  [self registerPropertyWithStore:propertyStore];
}

- (void)registerNonObjectPropertyWithGetter:(SEL)getter
                                     setter:(SEL)setter
                                       type:(const char*)type {
  [self registerPropertyWithStore:[[CRWNonObjectPropertyStore alloc]
                                      initWithGetter:getter
                                              setter:setter
                                                type:type]];
}

- (void)registerPropertyWithStore:(CRWPropertyStore*)store {
  [_propertyStores addObject:store];
  _propertyStoreByGetter[[NSValue valueWithPointer:store.getter]] = store;
  _propertyStoreBySetter[[NSValue valueWithPointer:store.setter]] = store;
}

- (void)savePropertiesFromObject:(id)object {
  for (CRWPropertyStore* propertyStore in _propertyStores) {
    // Invoke the getter against |object|.
    NSMethodSignature* signature =
        [object methodSignatureForSelector:propertyStore.getter];
    NSInvocation* invocation =
        [NSInvocation invocationWithMethodSignature:signature];
    invocation.selector = propertyStore.getter;
    [invocation invokeWithTarget:object];

    // Store the return value in the property store.
    NSMutableData* data = [NSMutableData dataWithLength:propertyStore.size];
    [invocation getReturnValue:data.mutableBytes];
    [propertyStore setValueFromBytes:data.bytes];
  }
}

- (void)loadPropertiesToObject:(id)object {
  for (CRWPropertyStore* propertyStore in _propertyStores) {
    if (!propertyStore.hasValue) {
      continue;
    }

    // Invoke the setter against |object| with the first argument set to the
    // value in the property store.
    NSMethodSignature* signature =
        [object methodSignatureForSelector:propertyStore.setter];
    NSInvocation* invocation =
        [NSInvocation invocationWithMethodSignature:signature];
    invocation.selector = propertyStore.setter;
    // const_cast is necessary because the first parameter of
    // -setArgument:atIndex: is somehow typed void* instead of const void*. But
    // it shouldn't modify its content in reality. The first argument is at
    // index 2 because the index 0 and 1 are for self and _cmd.
    [invocation setArgument:const_cast<void*>(propertyStore.valueBytes)
                    atIndex:2];
    [invocation invokeWithTarget:object];
  }
}

- (void)clearValues {
  for (CRWPropertyStore* propertyStore in _propertyStores) {
    [propertyStore clearValue];
  }
}

- (BOOL)forwardInvocationToPropertiesStore:(NSInvocation*)invocation {
  CRWPropertyStore* getterPropertyStore =
      _propertyStoreByGetter[[NSValue valueWithPointer:invocation.selector]];
  if (getterPropertyStore) {
    // This is the getter of a registered property. Return the stored value.
    return [self returnValueInPropertyStore:getterPropertyStore
                              forInvocation:invocation];
  }

  CRWPropertyStore* setterPropertyStore =
      _propertyStoreBySetter[[NSValue valueWithPointer:invocation.selector]];
  if (setterPropertyStore) {
    // This is the setter of a registered property. Retrieve the assigned value
    // and store it.
    return [self storeArgumentOfInvocation:invocation
                           toPropertyStore:setterPropertyStore];
  }

  return NO;
}

// Returns a value in |propertyStore| as a return value of |invocation|. Returns
// YES on success.
- (BOOL)returnValueInPropertyStore:(CRWPropertyStore*)propertyStore
                     forInvocation:(NSInvocation*)invocation {
  const char* returnType = invocation.methodSignature.methodReturnType;
  if (strcmp(returnType, propertyStore.type) != 0) {
    // It has NOTREACHED() followed by returning NO as an exception to the style
    // guide. This is because it should never reach here in the current version
    // of iOS, but it might reach here in a *future* version of iOS for the same
    // build, even though it should be quite rare. This happens if this class is
    // used for properties of an iOS standard class and Apple changes the type
    // of an existing property. When it happens, this method gives up handling
    // the specific property and continues execution by returning NO, rather
    // than causing buffer overflow.
    NOTREACHED()
        << "The return value type of "
        << NSStringFromSelector(invocation.selector).UTF8String << " ("
        << returnType
        << ") does not match the type registered to CRWPropertiesStore ("
        << propertyStore.type << ")";
    return NO;
  }

  // const_cast is necessary because the first parameter of -setReturnVaule:
  // is somehow typed void* instead of const void*. But it shouldn't modify
  // its content in reality.
  [invocation setReturnValue:const_cast<void*>(propertyStore.valueBytes)];
  return YES;
}

// Stores the only argument of |invocation| to |propertyStore|. Returns YES on
// success.
- (BOOL)storeArgumentOfInvocation:(NSInvocation*)invocation
                  toPropertyStore:(CRWPropertyStore*)propertyStore {
  NSUInteger numArguments = invocation.methodSignature.numberOfArguments;
  DCHECK_EQ(numArguments, 3UL)
      << NSStringFromSelector(invocation.selector).UTF8String
      << " must take exactly 3 arguments (including self and _cmd) "
         "but it takes "
      << numArguments;

  // The argument is at index 2 because the index 0 and 1 are for self and _cmd.
  const NSUInteger kArgumentIndex = 2;

  const char* argumentType =
      [invocation.methodSignature getArgumentTypeAtIndex:kArgumentIndex];
  if (strcmp(argumentType, propertyStore.type) != 0) {
    // It has NOTREACHED() followed by returning NO as an exception to the style
    // guide. This is because it should never reach here in the current version
    // of iOS, but it might reach here in a *future* version of iOS for the same
    // build, even though it should be quite rare. This happens if this class is
    // used for properties of an iOS standard class and Apple changes the type
    // of an existing property. When it happens, this method gives up handling
    // the specific property and continues execution by returning NO, rather
    // than causing buffer overflow.
    NOTREACHED()
        << "The argument type of "
        << NSStringFromSelector(invocation.selector).UTF8String << " ("
        << argumentType
        << ") does not match the type registered to CRWPropertiesStore ("
        << propertyStore.type << ")";
    return NO;
  }

  NSMutableData* data = [NSMutableData dataWithLength:propertyStore.size];
  [invocation getArgument:data.mutableBytes atIndex:kArgumentIndex];
  [propertyStore setValueFromBytes:data.bytes];
  return YES;
}

@end
