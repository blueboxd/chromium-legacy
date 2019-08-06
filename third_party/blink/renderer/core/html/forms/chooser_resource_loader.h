// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_CHOOSER_RESOURCE_LOADER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_CHOOSER_RESOURCE_LOADER_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ChooserResourceLoader {
  STATIC_ONLY(ChooserResourceLoader);

 public:
  // Returns the picker common stylesheet as a string.
  static String GetPickerCommonStyleSheet();

  // Returns the picker common javascript as a string.
  static String GetPickerCommonJS();

  // Returns the picker button stylesheet as a string.
  static String GetPickerButtonStyleSheet();

  // Returns the suggestion picker stylesheet as a string.
  static String GetSuggestionPickerStyleSheet();

  // Returns the suggestion picker javascript as a string.
  static String GetSuggestionPickerJS();

  // Returns the suggestion picker stylesheet as a string.
  static String GetCalendarPickerStyleSheet();

  // Returns the calendar picker refresh stylesheet as a string.
  static String GetCalendarPickerRefreshStyleSheet();

  // Returns the suggestion picker javascript as a string.
  static String GetCalendarPickerJS();

  // Returns the color suggestion picker stylesheet as a string.
  static String GetColorSuggestionPickerStyleSheet();

  // Returns the color suggestion picker javascript as a string.
  static String GetColorSuggestionPickerJS();

  // Returns the color picker stylesheet as a string.
  static String GetColorPickerStyleSheet();

  // Returns the color picker javascript as a string.
  static String GetColorPickerJS();

  // Returns the color picker common javascript as a string.
  static String GetColorPickerCommonJS();

  // Returns the list picker stylesheet as a string.
  static String GetListPickerStyleSheet();

  // Returns the list picker javascript as a string.
  static String GetListPickerJS();
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_CHOOSER_RESOURCE_LOADER_H_
