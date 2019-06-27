// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_RECURRENCE_RANKER_UTIL_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_RECURRENCE_RANKER_UTIL_H_

#include <memory>

#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_predictor.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker_config.pb.h"

namespace app_list {

// Returns a new, configured instance of the predictor defined in |config|.
std::unique_ptr<RecurrencePredictor> MakePredictor(
    const RecurrencePredictorConfigProto& config);

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_RECURRENCE_RANKER_UTIL_H_
