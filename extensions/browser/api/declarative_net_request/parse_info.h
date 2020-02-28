// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_PARSE_INFO_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_PARSE_INFO_H_

#include <stddef.h>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/optional.h"
#include "extensions/browser/api/declarative_net_request/constants.h"

namespace extensions {
namespace declarative_net_request {

// Holds the result of indexing a JSON ruleset.
class ParseInfo {
 public:
  // Creates a ParseInfo for a successful parse.
  ParseInfo();

  ParseInfo(ParseInfo&&);
  ParseInfo& operator=(ParseInfo&&);
  ~ParseInfo();

  // Rules which exceed the per rule regex memory limit. These are ignored
  // during indexing.
  void AddRegexLimitExceededRule(int rule_id);
  const std::vector<int>& regex_limit_exceeded_rules() const {
    return regex_limit_exceeded_rules_;
  }

  // |rule_id| is null when invalid.
  void SetError(ParseResult error_reason, const int* rule_id);

  bool has_error() const { return has_error_; }
  ParseResult error_reason() const {
    DCHECK(has_error_);
    return error_reason_;
  }
  const std::string& error() const {
    DCHECK(has_error_);
    return error_;
  }

 private:
  bool has_error_ = false;

  std::vector<int> regex_limit_exceeded_rules_;

  // Only valid iff |has_error_| is true.
  std::string error_;
  ParseResult error_reason_ = ParseResult::NONE;
};

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_PARSE_INFO_H_
