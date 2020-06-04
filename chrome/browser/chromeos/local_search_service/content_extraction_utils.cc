// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/local_search_service/content_extraction_utils.h"
#include <memory>

#include "base/i18n/case_conversion.h"
#include "base/i18n/unicodestring.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/icu/source/i18n/unicode/translit.h"

namespace local_search_service {

base::string16 Normalizer(const base::string16& word, bool remove_hyphen) {
  // Case folding.
  icu::UnicodeString source = icu::UnicodeString::fromUTF8(
      base::UTF16ToUTF8(base::i18n::FoldCase(word)));

  // Removes diacritic.
  UErrorCode status = U_ZERO_ERROR;
  UParseError parse_error;

  // Adds a rule to remove diacritic from text. Adds a few characters that are
  // not handled by ICU (ł > l; ø > o; đ > d).
  std::unique_ptr<icu::Transliterator> diacritic_remover =
      base::WrapUnique(icu::Transliterator::createFromRules(
          UNICODE_STRING_SIMPLE("RemoveDiacritic"),
          icu::UnicodeString::fromUTF8("::NFD; ::[:Nonspacing Mark:] Remove; "
                                       "::NFC; ł > l; ø > o; đ > d;"),
          UTRANS_FORWARD, parse_error, status));
  diacritic_remover->transliterate(source);

  // Removes hyphen.
  if (remove_hyphen) {
    // Hyphen characters list is taken from here: http://jkorpela.fi/dashes.html
    // U+002D(-), U+007E(~), U+058A(֊), U+05BE(־), U+1806(᠆), U+2010(‐),
    // U+2011(‑), U+2012(‒), U+2013(–), U+2014(—), U+2015(―), U+2053(⁓),
    // U+207B(⁻), U+208B(₋), U+2212(−), U+2E3A(⸺ ), U+2E3B(⸻  ), U+301C(〜),
    // U+3030(〰), U+30A0(゠), U+FE58(﹘), U+FE63(﹣), U+FF0D(－).
    std::unique_ptr<icu::Transliterator> hyphen_remover =
        base::WrapUnique(icu::Transliterator::createFromRules(
            UNICODE_STRING_SIMPLE("RemoveHyphen"),
            icu::UnicodeString::fromUTF8(
                "::[-~֊־᠆‐‑‒–—―⁓⁻₋−⸺⸻〜〰゠﹘﹣－] Remove;"),
            UTRANS_FORWARD, parse_error, status));
    hyphen_remover->transliterate(source);
  }

  return base::i18n::UnicodeStringToString16(source);
}
}  // namespace local_search_service
