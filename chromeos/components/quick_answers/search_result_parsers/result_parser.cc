// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/search_result_parsers/result_parser.h"

#include "base/values.h"
#include "chromeos/components/quick_answers/search_result_parsers/unit_conversion_result_parser.h"

namespace chromeos {
namespace quick_answers {
namespace {

using base::Value;

}  // namespace

const Value* ResultParser::GetFirstListElement(const Value& value,
                                               const std::string& path) {
  const Value* entries = value.FindListPath(path);

  if (!entries) {
    // No list found.
    return nullptr;
  }

  auto list = entries->GetList();
  if (list.empty()) {
    // No valid dictionary entries found.
    return nullptr;
  }
  return &list[0];
}

// static
std::unique_ptr<ResultParser> ResultParserFactory::Create(
    int one_namespace_type) {
  switch (static_cast<ResultType>(one_namespace_type)) {
    case ResultType::kUnitCconverterResult:
      return std::make_unique<UnitConversionResultParser>();
      // TODO(llin): Add other result parsers.
  }

  return nullptr;
}

}  // namespace quick_answers
}  // namespace chromeos
