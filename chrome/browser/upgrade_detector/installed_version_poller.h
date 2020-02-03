// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPGRADE_DETECTOR_INSTALLED_VERSION_POLLER_H_
#define CHROME_BROWSER_UPGRADE_DETECTOR_INSTALLED_VERSION_POLLER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/upgrade_detector/get_installed_version.h"

class BuildState;

namespace base {
class TickClock;
}  // namespace base

// Periodically polls for the installed version of the browser in the
// background. Polling begins automatically after construction and continues
// until destruction. The discovered version is provided to the given BuildState
// on each poll.
class InstalledVersionPoller {
 public:
  // The default polling interval. This may be overridden for testing by
  // specifying some number of seconds via --check-for-update-interval=seconds.
  static const base::TimeDelta kDefaultPollingInterval;

  // Constructs an instance that will poll upon construction and periodically
  // thereafter.
  explicit InstalledVersionPoller(BuildState* build_state);

  // A type of callback that is run (in the background) to get the currently
  // installed version of the browser.
  using GetInstalledVersionCallback =
      base::RepeatingCallback<InstalledAndCriticalVersion()>;

  // A constructor for tests that provide a mock source of time and a mock
  // version getter.
  InstalledVersionPoller(BuildState* build_state,
                         GetInstalledVersionCallback get_installed_version,
                         const base::TickClock* tick_clock);
  InstalledVersionPoller(const InstalledVersionPoller&) = delete;
  InstalledVersionPoller& operator=(const InstalledVersionPoller&) = delete;
  ~InstalledVersionPoller();

  class ScopedDisableForTesting {
   public:
    ScopedDisableForTesting(ScopedDisableForTesting&&) noexcept = default;
    ScopedDisableForTesting& operator=(ScopedDisableForTesting&&) = default;
    ~ScopedDisableForTesting();

   private:
    friend class InstalledVersionPoller;
    ScopedDisableForTesting();
  };

  // Returns an object that prevents any newly created instances of
  // InstalledVersionPoller from doing any work for the duration of its
  // lifetime.
  static ScopedDisableForTesting MakeScopedDisableForTesting() {
    return ScopedDisableForTesting();
  }

 private:
  // Initiates a poll in the background.
  void Poll();
  void OnInstalledVersion(InstalledAndCriticalVersion installed_version);

  SEQUENCE_CHECKER(sequence_checker_);
  BuildState* const build_state_;
  const GetInstalledVersionCallback get_installed_version_;
  base::RetainingOneShotTimer timer_;
  base::WeakPtrFactory<InstalledVersionPoller> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UPGRADE_DETECTOR_INSTALLED_VERSION_POLLER_H_
