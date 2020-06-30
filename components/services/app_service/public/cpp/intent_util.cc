// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/intent_util.h"

#include "base/compiler_specific.h"
#include "base/optional.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace {

// Get the intent condition value based on the condition type.
base::Optional<std::string> GetIntentConditionValueByType(
    apps::mojom::ConditionType condition_type,
    const apps::mojom::IntentPtr& intent) {
  switch (condition_type) {
    case apps::mojom::ConditionType::kAction:
      return intent->action;
    case apps::mojom::ConditionType::kScheme:
      return intent->url.has_value()
                 ? base::Optional<std::string>(intent->url->scheme())
                 : base::nullopt;
    case apps::mojom::ConditionType::kHost:
      return intent->url.has_value()
                 ? base::Optional<std::string>(intent->url->host())
                 : base::nullopt;
    case apps::mojom::ConditionType::kPattern:
      return intent->url.has_value()
                 ? base::Optional<std::string>(intent->url->path())
                 : base::nullopt;
    case apps::mojom::ConditionType::kMimeType:
      return intent->mime_type;
  }
}

bool ComponentMatched(const std::string& component1,
                      const std::string& component2) {
  const char kWildCardAny[] = "*";
  return component1 == kWildCardAny || component2 == kWildCardAny ||
         component1 == component2;
}

// TODO(crbug.com/1092784): Handle file path with extension with mime type.
bool MimeTypeMatched(const std::string& mime_type1,
                     const std::string& mime_type2) {
  const char kMimeTypeSeparator[] = "/";

  std::vector<std::string> components1 =
      base::SplitString(mime_type1, kMimeTypeSeparator, base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);

  std::vector<std::string> components2 =
      base::SplitString(mime_type2, kMimeTypeSeparator, base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);

  const size_t kMimeTypeComponentSize = 2;
  if (components1.size() != kMimeTypeComponentSize ||
      components2.size() != kMimeTypeComponentSize) {
    return false;
  }

  // Both intent and intent filter can use wildcard for mime type.
  for (size_t i = 0; i < kMimeTypeComponentSize; i++) {
    if (!ComponentMatched(components1[i], components2[i])) {
      return false;
    }
  }
  return true;
}

}  // namespace

namespace apps_util {

const char kIntentActionView[] = "view";
const char kIntentActionSend[] = "send";
const char kIntentActionSendMultiple[] = "send_multiple";

apps::mojom::IntentPtr CreateIntentFromUrl(const GURL& url) {
  auto intent = apps::mojom::Intent::New();
  intent->action = kIntentActionView;
  intent->url = url;
  return intent;
}

bool ConditionValueMatches(
    const std::string& value,
    const apps::mojom::ConditionValuePtr& condition_value) {
  switch (condition_value->match_type) {
    // Fallthrough as kNone and kLiteral has same matching type.
    case apps::mojom::PatternMatchType::kNone:
    case apps::mojom::PatternMatchType::kLiteral:
      return value == condition_value->value;
    case apps::mojom::PatternMatchType::kPrefix:
      return base::StartsWith(value, condition_value->value,
                              base::CompareCase::INSENSITIVE_ASCII);
    case apps::mojom::PatternMatchType::kGlob:
      return MatchGlob(value, condition_value->value);
    case apps::mojom::PatternMatchType::kMimeType:
      return MimeTypeMatched(value, condition_value->value);
  }
}

bool IntentMatchesCondition(const apps::mojom::IntentPtr& intent,
                            const apps::mojom::ConditionPtr& condition) {
  base::Optional<std::string> value_to_match =
      GetIntentConditionValueByType(condition->condition_type, intent);
  if (!value_to_match.has_value()) {
    return false;
  }
  for (const auto& condition_value : condition->condition_values) {
    if (ConditionValueMatches(value_to_match.value(), condition_value)) {
      return true;
    }
  }
  return false;
}

bool IntentMatchesFilter(const apps::mojom::IntentPtr& intent,
                         const apps::mojom::IntentFilterPtr& filter) {
  // Intent matches with this intent filter when all of the existing conditions
  // match.
  for (const auto& condition : filter->conditions) {
    if (!IntentMatchesCondition(intent, condition)) {
      return false;
    }
  }
  return true;
}

// TODO(crbug.com/853604): For glob match, it is currently only for Android
// intent filters, so we will use the ARC intent filter implementation that is
// transcribed from Android codebase, to prevent divergence from Android code.
// This is now also used for mime type matching.
bool MatchGlob(const std::string& value, const std::string& pattern) {
#define GET_CHAR(s, i) ((UNLIKELY(i >= s.length())) ? '\0' : s[i])

  const size_t NP = pattern.length();
  const size_t NS = value.length();
  if (NP == 0) {
    return NS == 0;
  }
  size_t ip = 0, is = 0;
  char nextChar = GET_CHAR(pattern, 0);
  while (ip < NP && is < NS) {
    char c = nextChar;
    ++ip;
    nextChar = GET_CHAR(pattern, ip);
    const bool escaped = (c == '\\');
    if (escaped) {
      c = nextChar;
      ++ip;
      nextChar = GET_CHAR(pattern, ip);
    }
    if (nextChar == '*') {
      if (!escaped && c == '.') {
        if (ip >= (NP - 1)) {
          // At the end with a pattern match
          return true;
        }
        ++ip;
        nextChar = GET_CHAR(pattern, ip);
        // Consume everything until the next char in the pattern is found.
        if (nextChar == '\\') {
          ++ip;
          nextChar = GET_CHAR(pattern, ip);
        }
        do {
          if (GET_CHAR(value, is) == nextChar) {
            break;
          }
          ++is;
        } while (is < NS);
        if (is == NS) {
          // Next char in the pattern didn't exist in the match.
          return false;
        }
        ++ip;
        nextChar = GET_CHAR(pattern, ip);
        ++is;
      } else {
        // Consume only characters matching the one before '*'.
        do {
          if (GET_CHAR(value, is) != c) {
            break;
          }
          ++is;
        } while (is < NS);
        ++ip;
        nextChar = GET_CHAR(pattern, ip);
      }
    } else {
      if (c != '.' && GET_CHAR(value, is) != c)
        return false;
      ++is;
    }
  }

  if (ip >= NP && is >= NS) {
    // Reached the end of both strings
    return true;
  }

  // One last check: we may have finished the match string, but still have a
  // '.*' at the end of the pattern, which is still a match.
  if (ip == NP - 2 && GET_CHAR(pattern, ip) == '.' &&
      GET_CHAR(pattern, ip + 1) == '*') {
    return true;
  }

  return false;

#undef GET_CHAR
}

}  // namespace apps_util
