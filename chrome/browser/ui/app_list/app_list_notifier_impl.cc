// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_notifier_impl.h"

#include "base/time/time.h"
#include "base/timer/timer.h"

namespace {

// TODO(crbug.com/1076270): Finalize a value for this, and possibly use
// different values for different UI surfaces.
constexpr base::TimeDelta kImpressionTimer = base::TimeDelta::FromSeconds(1);

}  // namespace

AppListNotifierImpl::AppListNotifierImpl() = default;
AppListNotifierImpl::~AppListNotifierImpl() = default;

void AppListNotifierImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AppListNotifierImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AppListNotifierImpl::NotifyLaunch(ash::SearchResultDisplayType location,
                                       const std::string& result) {
  launched_result_ = result;
  // TODO(crbug.com/1076270): A known issue here is that a launch of an app from
  // the app tiles after an impression will trigger an abandon of the results
  // list. It's unclear whether this is desirable and, if not, whether the
  // 'deduplication' should be handled in the notifier or its observers.
  DoStateTransition(location, State::kLaunched);
}

void AppListNotifierImpl::NotifyResultsUpdated(
    ash::SearchResultDisplayType location,
    const std::vector<std::string>& results) {
  results_[location] = results;
}

void AppListNotifierImpl::NotifySearchQueryChanged(
    const base::string16& query) {
  query_ = query;
  DoStateTransition(Location::kList, State::kShown);
  DoStateTransition(Location::kTile, State::kShown);
}

void AppListNotifierImpl::NotifyUIStateChanged(ash::AppListViewState view) {
  view_ = view;

  if (view == View::kHalf || view == View::kFullscreenSearch) {
    DoStateTransition(Location::kList, State::kShown);
    DoStateTransition(Location::kTile, State::kShown);
  } else {
    DoStateTransition(Location::kList, State::kNone);
    DoStateTransition(Location::kTile, State::kNone);
  }

  if (view == View::kPeeking || view == View::kFullscreenAllApps) {
    DoStateTransition(Location::kChip, State::kShown);
  } else {
    DoStateTransition(Location::kChip, State::kNone);
  }
}

void AppListNotifierImpl::RestartTimer(Location location) {
  if (timers_.find(location) == timers_.end()) {
    timers_[location] = std::make_unique<base::OneShotTimer>();
  }

  auto& timer = timers_[location];
  if (timer->IsRunning()) {
    timer->Stop();
  }
  // base::Unretained is safe here because the timer is a member of |this|, and
  // OneShotTimer cancels its timer on destruction.
  timer->Start(FROM_HERE, kImpressionTimer,
               base::BindOnce(&AppListNotifierImpl::OnTimerFinished,
                              base::Unretained(this), location));
}

void AppListNotifierImpl::StopTimer(Location location) {
  const auto it = timers_.find(location);
  if (it != timers_.end() && it->second->IsRunning()) {
    it->second->Stop();
  }
}

void AppListNotifierImpl::OnTimerFinished(Location location) {
  DoStateTransition(location, State::kSeen);
}

void AppListNotifierImpl::DoStateTransition(Location location,
                                            State new_state) {
  const State old_state = states_[location];

  // Update most recent state. We special-case kLaunched, which is a temporary
  // state that immediately becomes kNone because the launcher closes after a
  // launch.
  if (new_state == State::kLaunched) {
    states_[location] = State::kNone;
  } else {
    states_[location] = new_state;
  }

  // The following 5 overlapping cases are equivalent to the 8 explicit cases in
  // the header comment.

  // Restart timer on * -> kShown
  if (new_state == State::kShown) {
    RestartTimer(location);
  }

  // Stop timer on kShown -> {kNone, kLaunch}.
  if (old_state == State::kShown &&
      (new_state == State::kNone || new_state == State::kLaunched)) {
    StopTimer(location);
  }

  // Notify of impression on kShown -> {kSeen, kLaunched}.
  if (old_state == State::kShown &&
      (new_state == State::kSeen || new_state == State::kLaunched)) {
    for (auto& observer : observers_) {
      observer.OnImpression(location, results_[location]);
    }
  }

  // Notify of launch on * -> kLaunch.
  if (new_state == State::kLaunched) {
    for (auto& observer : observers_) {
      observer.OnLaunch(location, launched_result_, results_[location]);
    }
  }

  // Notify of abandon on kSeen -> {kNone, kShown}
  if (old_state == State::kSeen &&
      (new_state == State::kNone || new_state == State::kShown)) {
    for (auto& observer : observers_) {
      observer.OnAbandon(location, results_[location]);
    }
  }
}
