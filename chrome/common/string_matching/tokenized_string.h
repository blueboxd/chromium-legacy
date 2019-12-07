// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_STRING_MATCHING_TOKENIZED_STRING_H_
#define CHROME_COMMON_STRING_MATCHING_TOKENIZED_STRING_H_

#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/gfx/range/range.h"

// TokenizedString takes a string and breaks it down into token words. It
// first breaks using BreakIterator to get all the words. Then it breaks
// the words again at camel case boundaries and alpha/number boundaries.
class TokenizedString {
 public:
  typedef std::vector<base::string16> Tokens;
  typedef std::vector<gfx::Range> Mappings;

  explicit TokenizedString(const base::string16& text);
  ~TokenizedString();

  const base::string16& text() const { return text_; }
  const Tokens& tokens() const { return tokens_; }
  const Mappings& mappings() const { return mappings_; }

 private:
  void Tokenize();

  // Input text.
  const base::string16 text_;

  // Broken down tokens and the index mapping of tokens in original string.
  Tokens tokens_;
  Mappings mappings_;

  DISALLOW_COPY_AND_ASSIGN(TokenizedString);
};

#endif  // CHROME_COMMON_STRING_MATCHING_TOKENIZED_STRING_H_
