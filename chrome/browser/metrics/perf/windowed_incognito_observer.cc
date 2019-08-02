// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/perf/windowed_incognito_observer.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"

namespace metrics {

std::unique_ptr<WindowedIncognitoObserver>
WindowedIncognitoMonitor::CreateObserver() {
  base::AutoLock lock(lock_);
  return std::make_unique<WindowedIncognitoObserver>(
      this, num_incognito_window_opened_);
}

WindowedIncognitoObserver::WindowedIncognitoObserver(
    WindowedIncognitoMonitor* monitor,
    uint64_t num_incognito_window_opened)
    : windowed_incognito_monitor_(monitor),
      num_incognito_window_opened_(num_incognito_window_opened) {}

bool WindowedIncognitoObserver::IncognitoLaunched() const {
  return windowed_incognito_monitor_->IncognitoLaunched(
      num_incognito_window_opened_);
}

bool WindowedIncognitoObserver::IncognitoActive() const {
  return windowed_incognito_monitor_->IncognitoActive();
}

WindowedIncognitoMonitor::WindowedIncognitoMonitor()
    : num_active_incognito_windows_(0), num_incognito_window_opened_(0) {
  BrowserList::AddObserver(this);

  // No need to acquire |lock_| because no observer has been created yet.
  // Iterate over the BrowserList to get the current value of
  // |num_active_incognito_windows_|.
  for (auto* window : *BrowserList::GetInstance())
    if (window->profile()->IsOffTheRecord())
      num_active_incognito_windows_++;
}

WindowedIncognitoMonitor::~WindowedIncognitoMonitor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BrowserList::RemoveObserver(this);
}

bool WindowedIncognitoMonitor::IncognitoActive() const {
  base::AutoLock lock(lock_);
  return num_active_incognito_windows_ > 0;
}

bool WindowedIncognitoMonitor::IncognitoLaunched(
    uint64_t prev_num_incognito_opened) const {
  base::AutoLock lock(lock_);
  // Whether there is any incognito window opened after the observer was
  // created.
  return prev_num_incognito_opened < num_incognito_window_opened_;
}

void WindowedIncognitoMonitor::OnBrowserAdded(Browser* browser) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::AutoLock lock(lock_);
  if (!browser->profile()->IsOffTheRecord())
    return;

  num_active_incognito_windows_++;
  num_incognito_window_opened_++;
}

void WindowedIncognitoMonitor::OnBrowserRemoved(Browser* browser) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::AutoLock lock(lock_);
  if (!browser->profile()->IsOffTheRecord())
    return;

  DCHECK(num_active_incognito_windows_ > 0);
  num_active_incognito_windows_--;
}

}  // namespace metrics
