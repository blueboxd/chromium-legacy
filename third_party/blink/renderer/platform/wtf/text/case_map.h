// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_CASE_MAP_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_CASE_MAP_H_

#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"
#include "third_party/blink/renderer/platform/wtf/wtf_export.h"

namespace WTF {

class TextOffsetMap;

// This class performs the full Unicode case-mapping.
//
// See LowerASCII/UpperASCII() variants for faster, ASCII-only,
// locale-independent case-mapping.
class WTF_EXPORT CaseMap {
 public:
  // This is a specialized locale class that holds normalized locale for
  // |CaseMap|.
  class WTF_EXPORT WTF_EXPORT Locale {
   public:
    Locale() : case_map_locale_(nullptr) {}
    explicit Locale(const AtomicString& locale);

   private:
    const char* case_map_locale_;

    static const char* turkic_or_azeri_;
    static const char* greek_;
    static const char* lithuanian_;

    friend class CaseMap;
  };

  // Construct from the normalized locale.
  explicit CaseMap(const Locale& locale)
      : case_map_locale_(locale.case_map_locale_) {}

  // Construct from a locale string. The given locale is normalized.
  explicit CaseMap(const AtomicString& locale) : CaseMap(Locale(locale)) {}

  String ToLower(const String& source) const;
  String ToUpper(const String& source) const;

  scoped_refptr<StringImpl> ToLower(StringImpl* source) const;
  scoped_refptr<StringImpl> ToUpper(StringImpl* source) const;

  String ToLower(const String& source, TextOffsetMap* offset_map) const;
  String ToUpper(const String& source, TextOffsetMap* offset_map) const;

  UChar32 ToUpper(UChar32 c) const;

 private:
  const char* case_map_locale_;
};

}  // namespace WTF

using WTF::CaseMap;

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_CASE_MAP_H_
