// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOCAL_SEARCH_SERVICE_CONTENT_EXTRACTION_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_LOCAL_SEARCH_SERVICE_CONTENT_EXTRACTION_UTILS_H_

#include "base/strings/string16.h"

namespace local_search_service {

// Checks if a word is a stopword given a locale. Locale will be in the
// following format: language-country@variant (country and variant are
// optional).
bool IsStopword(const base::string16& word, const std::string& locale);

// Returns a normalized version of a string16: removes diacritics, convert to
// lower-case and possibly remove hyphen from the text (set to true by default).
base::string16 Normalizer(const base::string16& word,
                          bool remove_hyphen = true);

}  // namespace local_search_service
#endif  // CHROME_BROWSER_CHROMEOS_LOCAL_SEARCH_SERVICE_CONTENT_EXTRACTION_UTILS_H_
