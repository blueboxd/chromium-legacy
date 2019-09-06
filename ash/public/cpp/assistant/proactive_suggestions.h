// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASSISTANT_PROACTIVE_SUGGESTIONS_H_
#define ASH_PUBLIC_CPP_ASSISTANT_PROACTIVE_SUGGESTIONS_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "base/hash/hash.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"

namespace ash {

// An immutable data structure to represent a renderable set of proactive
// suggestions. Note that instances of ProactiveSuggestions are considered
// equivalent if they hash to the same value.
class ASH_PUBLIC_EXPORT ProactiveSuggestions
    : public base::RefCounted<ProactiveSuggestions> {
 public:
  static const int kCategoryUnknown = 0;

  // Constructs an instance with the specified |category|, |description|,
  // |search_query|, and renderable |html| content. Note that |category| is an
  // opaque int that is provided by the proactive suggestions server to
  // represent the category of the associated content (e.g. news, shopping).
  ProactiveSuggestions(int category,
                       std::string&& description,
                       std::string&& search_query,
                       std::string&& html);

  // Returns true if |a| is considered equivalent to |b|.
  static bool AreEqual(const ProactiveSuggestions* a,
                       const ProactiveSuggestions* b);

  // Returns a fast hash representation of the given |proactive_suggestions|.
  static size_t ToHash(const ProactiveSuggestions* proactive_suggestions);

  int category() const { return category_; }
  const std::string& description() const { return description_; }
  const std::string& search_query() const { return search_query_; }
  const std::string& html() const { return html_; }

 private:
  // Destruction only allowed by base::RefCounted<ProactiveSuggestions>.
  friend class base::RefCounted<ProactiveSuggestions>;
  ~ProactiveSuggestions();

  const int category_;
  const std::string description_;
  const std::string search_query_;
  const std::string html_;

  DISALLOW_COPY_AND_ASSIGN(ProactiveSuggestions);
};

ASH_PUBLIC_EXPORT bool operator==(const ProactiveSuggestions& lhs,
                                  const ProactiveSuggestions& rhs);

}  // namespace ash

namespace std {

template <>
struct hash<::ash::ProactiveSuggestions> {
  size_t operator()(
      const ::ash::ProactiveSuggestions& proactive_suggestions) const {
    size_t description = base::FastHash(proactive_suggestions.description());
    size_t search_query = base::FastHash(proactive_suggestions.search_query());
    size_t html = base::FastHash(proactive_suggestions.html());

    size_t hash = base::HashInts(proactive_suggestions.category(), description);
    hash = base::HashInts(hash, search_query);
    return base::HashInts(hash, html);
  }
};

}  // namespace std

#endif  // ASH_PUBLIC_CPP_ASSISTANT_PROACTIVE_SUGGESTIONS_H_
