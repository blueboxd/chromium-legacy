// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_BLINK_PREDICTION_FILTER_FACTORY_H_
#define UI_EVENTS_BLINK_PREDICTION_FILTER_FACTORY_H_

#include "ui/events/blink/prediction/input_filter.h"

namespace ui {

namespace input_prediction {

extern const char kFilterNameEmpty[];

enum class FilterType {
  kEmpty,
};
}  // namespace input_prediction

// FilterFactory is a class containing static public methods to create filters.
// It defines filters name and type constants. It also reads filter settings
// from fieldtrials if needed.
class FilterFactory {
 public:
  // Returns the FilterType associated to the given filter
  // name if found, otherwise returns kFilterTypeEmpty
  static input_prediction::FilterType GetFilterTypeFromName(
      const std::string& filter_name);

  // Returns the filter designed by its type.
  static std::unique_ptr<InputFilter> CreateFilter(
      input_prediction::FilterType filter_type);

 private:
  FilterFactory() = delete;
  ~FilterFactory() = delete;
};

}  // namespace ui

#endif  // UI_EVENTS_BLINK_PREDICTION_FILTER_FACTORY_H_