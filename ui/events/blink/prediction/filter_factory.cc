// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/prediction/filter_factory.h"
#include "ui/events/blink/prediction/empty_filter.h"

namespace ui {

namespace input_prediction {

const char kFilterNameEmpty[] = "empty_filter";

}  // namespace input_prediction

namespace {
using input_prediction::FilterType;
}  // namespace

FilterType FilterFactory::GetFilterTypeFromName(
    const std::string& filter_name) {
  return FilterType::kEmpty;
}

std::unique_ptr<InputFilter> FilterFactory::CreateFilter(
    input_prediction::FilterType filter_type) {
  return std::make_unique<EmptyFilter>();
}

}  // namespace ui
