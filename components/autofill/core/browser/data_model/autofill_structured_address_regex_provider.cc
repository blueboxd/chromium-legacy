// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_structured_address_regex_provider.h"
#include <utility>
#include "components/autofill/core/browser/data_model/autofill_structured_address_constants.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"

#include "base/notreached.h"

namespace autofill {

namespace structured_address {

StructuredAddressesRegExProvider::StructuredAddressesRegExProvider() = default;

// static
StructuredAddressesRegExProvider* StructuredAddressesRegExProvider::Instance() {
  static base::NoDestructor<StructuredAddressesRegExProvider>
      g_expression_provider;
  return g_expression_provider.get();
}

std::string StructuredAddressesRegExProvider::GetPattern(
    RegEx expression_identifier) {
  switch (expression_identifier) {
    case RegEx::kSingleWord:
      return kSingleWordRe;
  }
  NOTREACHED();
}

const RE2* StructuredAddressesRegExProvider::GetRegEx(
    RegEx expression_identifier) {
  base::AutoLock lock(lock_);
  auto it = cached_expressions_.find(expression_identifier);
  if (it == cached_expressions_.end()) {
    std::unique_ptr<const RE2> expression =
        BuildRegExFromPattern(GetPattern(expression_identifier));
    const RE2* expresstion_ptr = expression.get();
    cached_expressions_.emplace(expression_identifier, std::move(expression));
    return expresstion_ptr;
  }
  return it->second.get();
}

}  // namespace structured_address

}  // namespace autofill
