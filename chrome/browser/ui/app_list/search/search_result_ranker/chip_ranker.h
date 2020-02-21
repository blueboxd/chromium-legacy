// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_CHIP_RANKER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_CHIP_RANKER_H_

#include <map>
#include <memory>
#include <string>

#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/search/mixer.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_launch_data.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker.h"

namespace app_list {

// A ChipRanker provides a method for ranking suggestion chips in the Chrome OS
// Launcher. Given a list of SortedResults from the Mixer, the ChipRanker will
// rescore the chip items so that they are appropriately ranked, while
// preserving the original ordering of all groups of results.
//
// The ranking algorithm works as follows:
//   - Start with sorting the results already scored from the Mixer
//   - Take the top two app items, app1 and app2
//   - For each chip in the SortedResults list:
//      1. Rank app1, app2 and chip using a Dolphin model
//      2. Adjust chip score to sit in the correct position
//         relative to the two apps:
//        - If chip should be first
//            set chip score > app1 score
//        - If chip should sit between
//            set chip score > app2 score, < app1 score
//        - If chip is ranked last
//            take app2 and the next app item, app3, and continue
//          with same file.
class ChipRanker {
 public:
  explicit ChipRanker(Profile* profile);
  ~ChipRanker();

  ChipRanker(const ChipRanker&) = delete;
  ChipRanker& operator=(const ChipRanker&) = delete;

  // Train the ranker that compares the different result types.
  void Train(const AppLaunchData& app_launch_data);

  // Adjusts chip scores to fit in line with app scores using
  // ranking algorithm detailed above.
  void Rank(Mixer::SortedResults* results);

  // Set a fake ranker for tests.
  void SetForTest(std::unique_ptr<RecurrenceRanker> ranker);

  // Ranker generates scores used for re-arranging items, not
  // raw result scores.
  std::unique_ptr<RecurrenceRanker> ranker_;

 private:
  Profile* profile_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_CHIP_RANKER_H_
