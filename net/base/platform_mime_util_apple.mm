// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/platform_mime_util.h"

#import <Foundation/Foundation.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include <string>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_IOS)
#include <MobileCoreServices/MobileCoreServices.h>
#else
#include <CoreServices/CoreServices.h>
#endif  // BUILDFLAG(IS_IOS)

#if !defined(OS_IOS)
// SPI declaration; see the commentary in GetPlatformExtensionsForMimeType.
// iOS must not use any private API, per Apple guideline.

@interface NSURLFileTypeMappings : NSObject
+ (NSURLFileTypeMappings*)sharedMappings;
- (NSArray*)extensionsForMIMEType:(NSString*)mimeType;
@end
#endif  // !defined(OS_IOS)

namespace net {

bool PlatformMimeUtil::GetPlatformMimeTypeFromExtension(
    const base::FilePath::StringType& ext,
    std::string* result) const {
  std::string ext_nodot = ext;
  if (ext_nodot.length() >= 1 && ext_nodot[0] == L'.') {
    ext_nodot.erase(ext_nodot.begin());
  }

  // TODO(crbug.com/40189213): Remove iOS availability check when cronet
  // deployment target is bumped to 14.
  if (@available(macOS 11, iOS 14, *)) {
    UTType* uttype =
        [UTType typeWithFilenameExtension:base::SysUTF8ToNSString(ext_nodot)];
    // Dynamic UTTypes are made by the system in the event that there's a
    // non-identifiable mime type. For now, we should treat dynamic UTTypes as a
    // nonstandard format.
    if ([uttype isDynamic] || uttype.preferredMIMEType == nil) {
      return false;
    }
    *result = base::SysNSStringToUTF8(uttype.preferredMIMEType);
    return true;
  }
  *result = base::SysNSStringToUTF8(uttype.preferredMIMEType);
  return true;
}

bool PlatformMimeUtil::GetPlatformPreferredExtensionForMimeType(
    const std::string& mime_type,
    base::FilePath::StringType* ext) const {
  // TODO(crbug.com/40189213): Remove iOS availability check when cronet
  // deployment target is bumped to 14.
  if (@available(macOS 11, iOS 14, *)) {
    UTType* uttype =
        [UTType typeWithMIMEType:base::SysUTF8ToNSString(mime_type)];
    if ([uttype isDynamic] || uttype.preferredFilenameExtension == nil) {
      return false;
    }
    *ext = base::SysNSStringToUTF8(uttype.preferredFilenameExtension);
    return true;
  }
  *ext = base::SysNSStringToUTF8(uttype.preferredFilenameExtension);
  return true;
}

void PlatformMimeUtil::GetPlatformExtensionsForMimeType(
    const std::string& mime_type,
    std::unordered_set<base::FilePath::StringType>* extensions) const {
#if defined(OS_IOS)
  NSArray* extensions_list = nil;
#else
  // There is no API for this that uses UTIs. The WebKitSystemInterface call
  // WKGetExtensionsForMIMEType() is a thin wrapper around
  // [[NSURLFileTypeMappings sharedMappings] extensionsForMIMEType:], which is
  // used by Firefox as well.
  //
  // See:
  // http://mxr.mozilla.org/mozilla-central/search?string=extensionsForMIMEType
  // http://www.openradar.me/11384153
  // rdar://11384153
  NSArray* extensions_list = [[NSURLFileTypeMappings sharedMappings]
      extensionsForMIMEType:base::SysUTF8ToNSString(mime_type)];
#endif  // defined(OS_IOS)

  if (extensions_list) {
    for (NSString* extension in extensions_list) {
      extensions->insert(base::SysNSStringToUTF8(extension));
    }

    base::FilePath::StringType ext;
    if (GetPlatformPreferredExtensionForMimeType(mime_type, &ext)) {
      extensions->insert(ext);
    }
  }
#if (BUILDFLAG(IS_MAC) &&                                    \
     MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_VERSION_11_0) || \
    (BUILDFLAG(IS_IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_14_0)
  else {
    base::apple::ScopedCFTypeRef<CFStringRef> mime_ref(
        base::SysUTF8ToCFStringRef(mime_type));
    if (mime_ref) {
      bool extensions_found = false;
      base::apple::ScopedCFTypeRef<CFArrayRef> types(
          UTTypeCreateAllIdentifiersForTag(kUTTagClassMIMEType, mime_ref.get(),
                                           nullptr));
      if (types) {
        for (CFIndex i = 0; i < CFArrayGetCount(types.get()); i++) {
          base::apple::ScopedCFTypeRef<CFArrayRef> extensions_list;
          if (@available(macOS 10.10, *)) {
            extensions_list.reset(
                UTTypeCopyAllTagsWithClass(
                    base::apple::CFCast<CFStringRef>(
                        CFArrayGetValueAtIndex(types.get(), i)),
                    kUTTagClassFilenameExtension));
          }
          if (!extensions_list) {
            continue;
          }
          extensions_found = true;
          for (NSString* extension in base::apple::CFToNSPtrCast(
                   extensions_list.get())) {
            extensions->insert(base::SysNSStringToUTF8(extension));
          }
        }
      }
      if (extensions_found) {
        return;
      }
    }

  if (extensions_found) {
    return;
  }

  base::FilePath::StringType ext;
  if (GetPlatformPreferredExtensionForMimeType(mime_type, &ext)) {
    extensions->insert(ext);
  }
#endif
}

}  // namespace net
