// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"

#include <algorithm>
#include <map>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_regex_provider.h"
#include "components/autofill/core/browser/data_model/borrowed_transliterator.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {
namespace structured_address {

bool StructuredNamesEnabled() {
  return base::FeatureList::IsEnabled(
      features::kAutofillEnableSupportForMoreStructureInNames);
}

Re2RegExCache::Re2RegExCache() = default;

// static
Re2RegExCache* Re2RegExCache::Instance() {
  static base::NoDestructor<Re2RegExCache> g_re2regex_cache;
  return g_re2regex_cache.get();
}

const RE2* Re2RegExCache::GetRegEx(const std::string& pattern) {
  // For thread safety, acquire a lock to prevent concurrent access.
  base::AutoLock lock(lock_);

  auto it = regex_map_.find(pattern);
  if (it != regex_map_.end()) {
    const RE2* regex = it->second.get();
    return regex;
  }

  // Build the expression and verify it is correct.
  auto regex_ptr = BuildRegExFromPattern(pattern);

  // Insert the expression into the map, check the success and return the
  // pointer.
  auto result = regex_map_.emplace(pattern, std::move(regex_ptr));
  DCHECK(result.second);
  return result.first->second.get();
}

std::unique_ptr<const RE2> BuildRegExFromPattern(const std::string& pattern) {
  RE2::Options opt;
  // By default, patters are case sensitive.
  // Note that, the named-capture-group patterns build with
  // |CaptureTypeWithPattern()| apply a flag to make the matching case
  // insensitive.
  opt.set_case_sensitive(true);

  auto regex = std::make_unique<const RE2>(pattern, opt);

  if (!regex->ok()) {
    DEBUG_ALIAS_FOR_CSTR(pattern_copy, pattern.c_str(), 128);
    base::debug::DumpWithoutCrashing();
  }

  return regex;
}

bool HasCjkNameCharacteristics(const std::string& name) {
  return IsPartialMatch(name, RegEx::kMatchCjkNameCharacteristics);
}

bool HasMiddleNameInitialsCharacteristics(const std::string& middle_name) {
  return IsPartialMatch(middle_name,
                        RegEx::kMatchMiddleNameInitialsCharacteristics);
}

bool HasHispanicLatinxNameCharaceristics(const std::string& name) {
  // Check if the name contains one of the most common Hispanic/Latinx
  // last names.
  if (IsPartialMatch(name, RegEx::kMatchHispanicCommonNameCharacteristics))
    return true;

  // Check if it contains a last name conjunction.
  if (IsPartialMatch(name,
                     RegEx::kMatchHispanicLastNameConjuctionCharacteristics))
    return true;

  // If none of the above, there is not sufficient reason to assume this is a
  // Hispanic/Latinx name.
  return false;
}

bool ParseValueByRegularExpression(
    const std::string& value,
    const std::string& pattern,
    std::map<std::string, std::string>* result_map) {
  DCHECK(result_map);

  const RE2* regex = Re2RegExCache::Instance()->GetRegEx(pattern);

  return ParseValueByRegularExpression(value, regex, result_map);
}

bool ParseValueByRegularExpression(
    const std::string& value,
    const RE2* regex,
    std::map<std::string, std::string>* result_map) {
  if (!regex || !regex->ok())
    return false;

  // Get the number of capturing groups in the expression.
  // Note, the capturing group for the full match is not counted.
  size_t number_of_capturing_groups = regex->NumberOfCapturingGroups() + 1;

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
  if (!RE2::FullMatchN(value, *regex, match_results_ptr.data(),
                       number_of_capturing_groups - 1))
    return false;

  // If successful, write the values into the results map.
  // Note, the capturing group for the full match creates an off-by-one scenario
  // in the indexing.
  for (auto named_group : regex->NamedCapturingGroups())
    (*result_map)[named_group.first] =
        std::move(results.at(named_group.second - 1));

  return true;
}

bool IsPartialMatch(const std::string& value, RegEx regex) {
  return IsPartialMatch(
      value, StructuredAddressesRegExProvider::Instance()->GetRegEx(regex));
}

bool IsPartialMatch(const std::string& value, const std::string& pattern) {
  return IsPartialMatch(value, Re2RegExCache::Instance()->GetRegEx(pattern));
}

bool IsPartialMatch(const std::string& value, const RE2* expression) {
  return RE2::PartialMatch(value, *expression);
}

std::vector<std::string> GetAllPartialMatches(const std::string& value,
                                              const std::string& pattern) {
  const RE2* regex = Re2RegExCache::Instance()->GetRegEx(pattern);
  if (!regex || !regex->ok())
    return {};
  re2::StringPiece input(value);
  std::string match;
  std::vector<std::string> matches;
  while (re2::RE2::FindAndConsume(&input, *regex, &match)) {
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

std::string CaptureTypeWithPattern(
    const ServerFieldType& type,
    std::initializer_list<base::StringPiece> pattern_span_initializer_list) {
  return CaptureTypeWithPattern(type, pattern_span_initializer_list,
                                CaptureOptions());
}

std::string CaptureTypeWithPattern(
    const ServerFieldType& type,
    std::initializer_list<base::StringPiece> pattern_span_initializer_list,
    const CaptureOptions& options) {
  return CaptureTypeWithPattern(
      type, base::StrCat(base::make_span(pattern_span_initializer_list)),
      options);
}

std::string CaptureTypeWithPattern(const ServerFieldType& type,
                                   const std::string& pattern,
                                   const CaptureOptions& options) {
  std::string quantifier;
  switch (options.quantifier) {
    // Makes the match optional.
    case MATCH_OPTIONAL:
      quantifier = "?";
      break;
    // Makes the match lazy meaning that it is avoided if possible.
    case MATCH_LAZY_OPTIONAL:
      quantifier = "??";
      break;
    // Makes the match required.
    case MATCH_REQUIRED:
      quantifier = "";
  }

  // By adding an "i" in the first group, the capturing is case insensitive.
  // Allow multiple separators to support the ", " case.
  return base::StrCat({"(?i:(?P<", AutofillType(type).ToString(), ">", pattern,
                       ")(?:", options.separator, ")+)", quantifier});
}

std::string CaptureTypeWithPattern(const ServerFieldType& type,
                                   const std::string& pattern) {
  return CaptureTypeWithPattern(type, pattern, CaptureOptions());
}

base::string16 NormalizeValue(const base::string16& value) {
  return RemoveDiacriticsAndConvertToLowerCase(
      base::CollapseWhitespace(value, /*trim_sequence_with_line_breaks=*/true));
}

bool AreStringTokenEquivalent(const base::string16& one,
                              const base::string16& other) {
  return AreSortedTokensEqual(TokenizeValue(one), TokenizeValue(other));
}

bool AreSortedTokensEqual(const std::vector<base::string16>& first,
                          const std::vector<base::string16>& second) {
  // It is assumed that the vectors are sorted.
  DCHECK(std::is_sorted(first.begin(), first.end()) &&
         std::is_sorted(second.begin(), second.end()));
  // If there is a different number of tokens, it can't be a permutation.
  if (first.size() != second.size())
    return false;
  // Return true if both vectors are component-wise equal.
  return std::equal(first.begin(), first.end(), second.begin());
}

std::vector<base::string16> TokenizeValue(const base::string16 value) {
  // Canonicalize the value.
  base::string16 cannonicalized_value = NormalizeValue(value);

  // CJK names are a special case and are tokenized by character without the
  // separators.
  std::vector<base::string16> tokens;
  if (HasCjkNameCharacteristics(base::UTF16ToUTF8(value))) {
    tokens.reserve(value.size());
    for (size_t i = 0; i < value.size(); i++) {
      base::string16 cjk_separators = base::UTF8ToUTF16("・·　 ");
      if (cjk_separators.find(value.substr(i, 1)) == base::string16::npos) {
        tokens.emplace_back(value.substr(i, 1));
      }
    }
  } else {
    // Split it by white spaces and commas into non-empty values.
    tokens =
        base::SplitString(cannonicalized_value, base::ASCIIToUTF16(", "),
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  }
  // Sort the tokens lexicographically.
  std::sort(tokens.begin(), tokens.end());

  return tokens;
}

}  // namespace structured_address
}  // namespace autofill
