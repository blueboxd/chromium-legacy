// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/assistant/proactive_suggestions.h"

namespace ash {

ProactiveSuggestions::ProactiveSuggestions(int category,
                                           std::string&& description,
                                           std::string&& search_query,
                                           std::string&& html)
    : category_(category),
      description_(std::move(description)),
      search_query_(std::move(search_query)),
      html_(std::move(html)) {}

ProactiveSuggestions::~ProactiveSuggestions() = default;

// static
bool ProactiveSuggestions::AreEqual(const ProactiveSuggestions* a,
                                    const ProactiveSuggestions* b) {
  return ProactiveSuggestions::ToHash(a) == ProactiveSuggestions::ToHash(b);
}

// static
size_t ProactiveSuggestions::ToHash(
    const ProactiveSuggestions* proactive_suggestions) {
  return proactive_suggestions
             ? std::hash<ProactiveSuggestions>{}(*proactive_suggestions)
             : static_cast<size_t>(0);
}

bool operator==(const ProactiveSuggestions& lhs,
                const ProactiveSuggestions& rhs) {
  return ProactiveSuggestions::AreEqual(&lhs, &rhs);
}

}  // namespace ash
