// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_UTILS_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "third_party/re2/src/re2/re2.h"

namespace autofill {
namespace structured_address {

// A cache for compiled RE2 regular expressions.
class Re2ExpressionCache {
 public:
  Re2ExpressionCache& operator=(const Re2ExpressionCache&) = delete;
  Re2ExpressionCache(const Re2ExpressionCache&) = delete;
  ~Re2ExpressionCache() = delete;

  // Returns a singleton instance.
  static Re2ExpressionCache* Instance();

  // Returns a pointer to a constant compiled expression that matches |pattern|
  // case-insensitively.
  const RE2* GetExpression(const std::string& pattern);

#ifdef UNIT_TEST
  // Returns true if the compiled regular expression corresponding to |pattern|
  // is cached.
  bool IsExpressionCachedForTesting(const std::string& pattern) {
    return expression_map_.count(pattern) > 0;
  }
#endif

 private:
  Re2ExpressionCache();

  // Since the constructor is private, |base::NoDestructor| must be friend to be
  // allowed to construct the cache.
  friend class base::NoDestructor<Re2ExpressionCache>;

  // Stores a compiled regular expression keyed by its corresponding |pattern|.
  std::map<std::string, std::unique_ptr<const RE2>> expression_map_;

  // A lock to prevent concurrent access to the map.
  base::Lock lock_;
};

// Parses |value| with an regular expression defined by |pattern|.
// Returns true on success meaning that the expressions is fully matched.
// The matching results are written into the supplied |result_map|, keyed by the
// name of the capture group with the captured substrings as the value.
bool ParseValueByRegularExpression(
    const std::string& value,
    const std::string& pattern,
    std::map<std::string, std::string>* result_map);

// Same as above, but accepts a compiled regular expression instead of the
// pattern.
bool ParseValueByRegularExpression(
    const std::string& value,
    const RE2* expression,
    std::map<std::string, std::string>* result_map);

// Returns a compiled case insensitive regular expression for |pattern|.
std::unique_ptr<const RE2> BuildExpressionFromPattern(std::string pattern);

// Returns true if |value| can be matched with |pattern|.
bool IsPartialMatch(const std::string& value, const std::string& pattern);

// Returns a vector that contains all partial matches of |pattern| in |value|;
std::vector<std::string> GetAllPartialMatches(const std::string& value,
                                              const std::string& pattern);

// Extracts all placeholders of the format ${PLACEHOLDER} in |value|.
std::vector<std::string> ExtractAllPlaceholders(const std::string& value);

// Returns |value| as a placeholder token: ${value}.
std::string GetPlaceholderToken(const std::string& value);

}  // namespace structured_address

}  // namespace autofill
#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_UTILS_H_
