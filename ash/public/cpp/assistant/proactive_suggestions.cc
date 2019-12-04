// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/assistant/proactive_suggestions.h"

#include "base/hash/hash.h"

namespace ash {

namespace {

// Helpers ---------------------------------------------------------------------

size_t GetHash(int category,
               const std::string& description,
               const std::string& search_query,
               const std::string& html,
               const std::string& rich_entry_point_html) {
  size_t hash = base::HashInts(category, base::FastHash(description));
  hash = base::HashInts(hash, base::FastHash(search_query));
  hash = base::HashInts(hash, base::FastHash(html));
  return base::HashInts(hash, base::FastHash(rich_entry_point_html));
}

}  // namespace

// ProactiveSuggestions --------------------------------------------------------

ProactiveSuggestions::ProactiveSuggestions(int category,
                                           std::string&& description,
                                           std::string&& search_query,
                                           std::string&& html,
                                           std::string&& rich_entry_point_html)
    : hash_(GetHash(category,
                    description,
                    search_query,
                    html,
                    rich_entry_point_html)),
      category_(category),
      description_(std::move(description)),
      search_query_(std::move(search_query)),
      html_(std::move(html)),
      rich_entry_point_html_(std::move(rich_entry_point_html)) {}

ProactiveSuggestions::~ProactiveSuggestions() = default;

// static
bool ProactiveSuggestions::AreEqual(const ProactiveSuggestions* a,
                                    const ProactiveSuggestions* b) {
  return ProactiveSuggestions::ToHash(a) == ProactiveSuggestions::ToHash(b);
}

// static
size_t ProactiveSuggestions::ToHash(
    const ProactiveSuggestions* proactive_suggestions) {
  return proactive_suggestions ? proactive_suggestions->hash()
                               : static_cast<size_t>(0);
}

bool operator==(const ProactiveSuggestions& lhs,
                const ProactiveSuggestions& rhs) {
  return ProactiveSuggestions::AreEqual(&lhs, &rhs);
}

}  // namespace ash
