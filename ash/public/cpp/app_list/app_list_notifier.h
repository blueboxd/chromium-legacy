// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_APP_LIST_APP_LIST_NOTIFIER_H_
#define ASH_PUBLIC_CPP_APP_LIST_APP_LIST_NOTIFIER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/ash_public_export.h"
#include "base/observer_list_types.h"

namespace ash {

// A notifier interface implemented in Chrome and called from Ash, which allows
// objects in Chrome to observe state changes in Ash. Its main use is to signal
// events related to metrics and logging: search result impressions, abandons,
// and launches.
//
// TODO(crbug.com/1076270): Add a description of the impression/abandon/launch
// state machine once it is finalized.
class ASH_PUBLIC_EXPORT AppListNotifier {
 public:
  class Observer : public base::CheckedObserver {
    // Called when |results| have been displayed for |kImpressionTimeMillis|.
    virtual void OnImpression(SearchResultDisplayType location,
                              const std::vector<std::string>& results) {}

    // Called when an impression occurred for |results|, and the user then moved
    // to a different UI surface. This includes closing the launcher, or
    // changing the search query. OnImpression is guaranteed to be called before
    // OnAbandon with |results|.
    virtual void OnAbandon(SearchResultDisplayType location,
                           const std::vector<std::string>& results) {}

    // Called when |result| is launched. OnImpression is not guaranteed to be
    // called before OnLaunch, because the user may launch |result| faster than
    // kImpressionTimeMillis.
    virtual void OnLaunch(SearchResultDisplayType location,
                          const std::string& result) {}
  };

  virtual ~AppListNotifier() = default;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Called to indicate a search |result| has been launched at the UI surface
  // |location|.
  virtual void NotifyLaunch(ash::SearchResultDisplayType location,
                            const std::string& result) = 0;

  // Called to indicate the results displayed in the |location| UI surface have
  // changed. |results| should contain a complete list of what is now shown.
  virtual void NotifyResultsUpdated(
      ash::SearchResultDisplayType location,
      const std::vector<std::string>& results) = 0;

  // Called to indicate the user has updated the search query to |query|.
  virtual void NotifySearchQueryChanged(const base::string16& query) = 0;

  // Called to indicate the UI state is now |state|.
  virtual void NotifyUIStateChanged(ash::AppListViewState state) = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_APP_LIST_APP_LIST_NOTIFIER_H_
