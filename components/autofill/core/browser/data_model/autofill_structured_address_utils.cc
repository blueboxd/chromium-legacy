// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"

#include <algorithm>
#include <map>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/strings/strcat.h"

namespace autofill {
namespace structured_address {

Re2ExpressionCache::Re2ExpressionCache() = default;

// static
Re2ExpressionCache* Re2ExpressionCache::Instance() {
  static base::NoDestructor<Re2ExpressionCache> g_re2regex_cache;
  return g_re2regex_cache.get();
}

const RE2* Re2ExpressionCache::GetExpression(const std::string& pattern) {
  // For thread safety, acquire a lock to prevent concurrent access.
  base::AutoLock lock(lock_);

  auto it = expression_map_.find(pattern);
  if (it != expression_map_.end()) {
    const RE2* expression = it->second.get();
    return expression;
  }

  // Build the expression and verify it is correct.
  auto expression_ptr = BuildExpressionFromPattern(pattern);

  // Insert the expression into the map, check the success and return the
  // pointer.
  auto result = expression_map_.emplace(pattern, std::move(expression_ptr));
  DCHECK(result.second);
  return result.first->second.get();
}

std::unique_ptr<const RE2> BuildExpressionFromPattern(std::string pattern) {
  RE2::Options opt;
  opt.set_case_sensitive(false);

  auto expression = std::make_unique<const RE2>(pattern, opt);

  if (!expression->ok()) {
    DEBUG_ALIAS_FOR_CSTR(pattern_copy, pattern.c_str(), 128);
    base::debug::DumpWithoutCrashing();
  }

  return expression;
}

bool ParseValueByRegularExpression(
    const std::string& value,
    const std::string& pattern,
    std::map<std::string, std::string>* result_map) {
  DCHECK(result_map);

  const RE2* expression =
      Re2ExpressionCache::Instance()->GetExpression(pattern);

  return ParseValueByRegularExpression(value, expression, result_map);
}

bool ParseValueByRegularExpression(
    const std::string& value,
    const RE2* expression,
    std::map<std::string, std::string>* result_map) {
  if (!expression || !expression->ok())
    return false;

  // Get the number of capturing groups in the expression.
  // Note, the capturing group for the full match is not counted.
  size_t number_of_capturing_groups = expression->NumberOfCapturingGroups() + 1;

  // Create result vectors to get the matches for the capturing groups.
  std::vector<std::string> results(number_of_capturing_groups);
  std::vector<RE2::Arg> match_results(number_of_capturing_groups);
  std::vector<RE2::Arg*> match_results_ptr(number_of_capturing_groups);

  // Note, the capturing group for the full match is not counted by
  // |NumberOfCapturingGroups|.
  for (size_t i = 0; i < number_of_capturing_groups; i++) {
    match_results[i] = &results[i];
    match_results_ptr[i] = &match_results[i];
  }

  // One capturing group is not counted since it holds the full match.
  if (!RE2::FullMatchN(value, *expression, match_results_ptr.data(),
                       number_of_capturing_groups - 1))
    return false;

  // If successful, write the values into the results map.
  // Note, the capturing group for the full match creates an off-by-one scenario
  // in the indexing.
  for (auto named_group : expression->NamedCapturingGroups())
    (*result_map)[named_group.first] =
        std::move(results.at(named_group.second - 1));

  return true;
}

bool IsPartialMatch(const std::string& value, const std::string& pattern) {
  const RE2* expression =
      Re2ExpressionCache::Instance()->GetExpression(pattern);
  if (!expression || !expression->ok())
    return false;

  return RE2::PartialMatch(value, *expression);
}

std::vector<std::string> GetAllPartialMatches(const std::string& value,
                                              const std::string& pattern) {
  const RE2* expression =
      Re2ExpressionCache::Instance()->GetExpression(pattern);
  if (!expression || !expression->ok())
    return {};
  re2::StringPiece input(value);
  std::string match;
  std::vector<std::string> matches;
  while (re2::RE2::FindAndConsume(&input, *expression, &match)) {
    matches.emplace_back(match);
  }
  return matches;
}

std::vector<std::string> ExtractAllPlaceholders(const std::string& value) {
  return GetAllPartialMatches(value, "\\${([\\w]+)}");
}

std::string GetPlaceholderToken(const std::string& value) {
  return base::StrCat({"${", value, "}"});
}

}  // namespace structured_address
}  // namespace autofill
