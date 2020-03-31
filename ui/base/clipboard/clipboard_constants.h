// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_CONSTANTS_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_CONSTANTS_H_

#include <string>

#include "base/component_export.h"
#include "build/build_config.h"

#if defined(OS_MACOSX)
#ifdef __OBJC__
@class NSString;
#else
class NSString;
#endif
#endif  // defined(OS_MACOSX)

namespace ui {

// Platform-Neutral MIME type constants.
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES) extern const char kMimeTypeText[];
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES) extern const char kMimeTypeTextUtf8[];
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES) extern const char kMimeTypeURIList[];
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern const char kMimeTypeDownloadURL[];
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern const char kMimeTypeMozillaURL[];
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES) extern const char kMimeTypeHTML[];
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES) extern const char kMimeTypeRTF[];
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES) extern const char kMimeTypePNG[];
#if !defined(OS_MACOSX)
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern const char kMimeTypeWebCustomData[];
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern const char kMimeTypeWebkitSmartPaste[];
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern const char kMimeTypePepperCustomData[];
#else
// MacOS-specific Uniform Type Identifiers.

// Pickled data.
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern NSString* const kWebCustomDataPboardType;
// Tells us if WebKit was the last to write to the pasteboard. There's no
// actual data associated with this type.
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern NSString* const kWebSmartPastePboardType;
// Pepper custom data format type.
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES)
extern NSString* const kPepperCustomDataPboardType;
#endif  // defined(OS_MACOSX)

#if defined(OS_ANDROID)
COMPONENT_EXPORT(UI_BASE_CLIPBOARD_TYPES) extern const char kMimeTypeImageURI[];
#endif  // defined(OS_ANDROID)

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_CONSTANTS_H_
