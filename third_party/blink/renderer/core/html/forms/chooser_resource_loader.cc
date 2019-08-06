// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/chooser_resource_loader.h"

#include "build/build_config.h"
#include "third_party/blink/public/resources/grit/blink_resources.h"
#include "third_party/blink/renderer/platform/data_resource_helper.h"

namespace blink {

String ChooserResourceLoader::GetSuggestionPickerStyleSheet() {
#if !defined(OS_ANDROID)
  return UncompressResourceAsString(IDR_SUGGESTION_PICKER_CSS);
#else
  NOTREACHED();
  return String();
#endif
}

String ChooserResourceLoader::GetSuggestionPickerJS() {
#if !defined(OS_ANDROID)
  return UncompressResourceAsString(IDR_SUGGESTION_PICKER_JS);
#else
  NOTREACHED();
  return String();
#endif
}

String ChooserResourceLoader::GetPickerButtonStyleSheet() {
#if !defined(OS_ANDROID)
  return UncompressResourceAsString(IDR_PICKER_BUTTON_CSS);
#else
  NOTREACHED();
  return String();
#endif
}

String ChooserResourceLoader::GetPickerCommonStyleSheet() {
#if !defined(OS_ANDROID)
  return UncompressResourceAsString(IDR_PICKER_COMMON_CSS);
#else
  NOTREACHED();
  return String();
#endif
}

String ChooserResourceLoader::GetPickerCommonJS() {
#if !defined(OS_ANDROID)
  return UncompressResourceAsString(IDR_PICKER_COMMON_JS);
#else
  NOTREACHED();
  return String();
#endif
}

String ChooserResourceLoader::GetCalendarPickerStyleSheet() {
#if !defined(OS_ANDROID)
  return UncompressResourceAsString(IDR_CALENDAR_PICKER_CSS);
#else
  NOTREACHED();
  return String();
#endif
}

String ChooserResourceLoader::GetCalendarPickerJS() {
#if !defined(OS_ANDROID)
  return UncompressResourceAsString(IDR_CALENDAR_PICKER_JS);
#else
  NOTREACHED();
  return String();
#endif
}

String ChooserResourceLoader::GetColorSuggestionPickerStyleSheet() {
#if !defined(OS_ANDROID)
  return UncompressResourceAsString(IDR_COLOR_SUGGESTION_PICKER_CSS);
#else
  NOTREACHED();
  return String();
#endif
}

String ChooserResourceLoader::GetColorSuggestionPickerJS() {
#if !defined(OS_ANDROID)
  return UncompressResourceAsString(IDR_COLOR_SUGGESTION_PICKER_JS);
#else
  NOTREACHED();
  return String();
#endif
}

String ChooserResourceLoader::GetColorPickerStyleSheet() {
#if !defined(OS_ANDROID)
  return UncompressResourceAsString(IDR_COLOR_PICKER_CSS);
#else
  NOTREACHED();
  return String();
#endif
}

String ChooserResourceLoader::GetCalendarPickerRefreshStyleSheet() {
#if !defined(OS_ANDROID)
  return UncompressResourceAsString(IDR_CALENDAR_PICKER_REFRESH_CSS);
#else
  NOTREACHED();
  return String();
#endif
}

String ChooserResourceLoader::GetColorPickerJS() {
#if !defined(OS_ANDROID)
  return UncompressResourceAsString(IDR_COLOR_PICKER_JS);
#else
  NOTREACHED();
  return String();
#endif
}

String ChooserResourceLoader::GetColorPickerCommonJS() {
#if !defined(OS_ANDROID)
  return UncompressResourceAsString(IDR_COLOR_PICKER_COMMON_JS);
#else
  NOTREACHED();
  return String();
#endif
}

String ChooserResourceLoader::GetListPickerStyleSheet() {
#if !defined(OS_ANDROID)
  return UncompressResourceAsString(IDR_LIST_PICKER_CSS);
#else
  NOTREACHED();
  return String();
#endif
}

String ChooserResourceLoader::GetListPickerJS() {
#if !defined(OS_ANDROID)
  return UncompressResourceAsString(IDR_LIST_PICKER_JS);
#else
  NOTREACHED();
  return String();
#endif
}

}  // namespace blink
