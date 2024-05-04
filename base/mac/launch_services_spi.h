// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MAC_LAUNCH_SERVICES_SPI_H_
#define BASE_MAC_LAUNCH_SERVICES_SPI_H_

#import <AppKit/AppKit.h>
#include <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>

// Private SPIs for using LaunchServices. Largely derived from usage of these
// in open source WebKit code [1] and some inspection of the LaunchServices
// binary, as well as AppKit's __NSWorkspaceOpenConfigurationGetLSOpenOptions.
//
// [1]
// https://github.com/WebKit/webkit/blob/main/Source/WebCore/PAL/pal/spi/cocoa/LaunchServicesSPI.h

extern "C" {

using LSASNRef = const struct CF_BRIDGED_TYPE(id) __LSASN*;

extern const CFStringRef _kLSOpenOptionActivateKey
    __attribute__((weak_import, weak));
extern const CFStringRef _kLSOpenOptionAddToRecentsKey
    __attribute__((weak_import, weak));
extern const CFStringRef _kLSOpenOptionArgumentsKey
    __attribute__((weak_import, weak));
extern const CFStringRef _kLSOpenOptionBackgroundLaunchKey
    __attribute__((weak_import, weak));
extern const CFStringRef _kLSOpenOptionHideKey
    __attribute__((weak_import, weak));
extern const CFStringRef _kLSOpenOptionPreferRunningInstanceKey
    __attribute__((weak_import, weak));

using _LSOpenCompletionHandler = void (^)(LSASNRef, Boolean, CFErrorRef);
void _LSOpenURLsWithCompletionHandler(
    CFArrayRef urls,
    CFURLRef application_url,
    CFDictionaryRef options,
    _LSOpenCompletionHandler completion_handler);

@interface NSRunningApplication ()
- (id)initWithApplicationSerialNumber:(LSASNRef)asn;
@end

}  // extern "C"

@interface NSWorkspaceOpenConfiguration (SPI)
@property(atomic, readwrite, setter=_setAdditionalLSOpenOptions:)
    NSDictionary* _additionalLSOpenOptions;
@end

#endif  // BASE_MAC_LAUNCH_SERVICES_SPI_H_
