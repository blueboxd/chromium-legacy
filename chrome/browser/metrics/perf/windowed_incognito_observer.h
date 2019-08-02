// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_PERF_WINDOWED_INCOGNITO_OBSERVER_H_
#define CHROME_BROWSER_METRICS_PERF_WINDOWED_INCOGNITO_OBSERVER_H_

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/ui/browser_list_observer.h"

class Browser;

namespace metrics {

class WindowedIncognitoMonitor;

// WindowedIncognitoObserver provides an interface for getting the current
// status of incognito windows and monitoring any new incognito windows after
// the observer is created. It can be used from any sequence.
//
// Example:
//
// // Create an WindowedIncognitoMonitor on the UI thread:
// auto windowed_incognito_monitor =
//     std:::make_unique<WindowedIncognitoMonitor>();
//
// // Use an observer from any sequence:
// auto observer = windowed_incognito_monitor->CreateObserver();
// bool active = observer->IncognitoActive();
// // |active| will be true if there is any active incognito window.
//
// // An incognito window is opened.
// bool launched = observer->IncognitoLaunched();
// EXPECT_TRUE(launched);
//
// // Destroy the monitor on the UI thread:
// windowed_incognito_monitor.reset();
class WindowedIncognitoObserver {
 public:
  explicit WindowedIncognitoObserver(WindowedIncognitoMonitor* monitor,
                                     uint64_t incognito_open_count);
  virtual ~WindowedIncognitoObserver() = default;

  // Made virtual for override in test.
  virtual bool IncognitoLaunched() const;
  bool IncognitoActive() const;

 private:
  WindowedIncognitoMonitor* windowed_incognito_monitor_;

  // The number of incognito windows that has been opened when the observer is
  // created.
  uint64_t num_incognito_window_opened_;

  DISALLOW_COPY_AND_ASSIGN(WindowedIncognitoObserver);
};

// WindowedIncognitoMonitor watches for any incognito window being opened or
// closed from the time it is instantiated to the time it is destroyed. The
// monitor is affine to the UI thread: instantiation, destruction and the
// BrowserListObserver callbacks are called on the UI thread. The other methods
// for creating and serving WindowedIncognitoObserver are thread-safe.
class WindowedIncognitoMonitor : public BrowserListObserver {
 public:
  WindowedIncognitoMonitor();
  ~WindowedIncognitoMonitor() override;

  // Returns an instance of WindowedIncognitoObserver that represents the
  // request for monitoring any incognito window launches from now on.
  std::unique_ptr<WindowedIncognitoObserver> CreateObserver();

 protected:
  // Making IncognitoActive() and IncognitoLaunched() only accessible from
  // WindowedIncognitoObserver;
  friend class WindowedIncognitoObserver;

  // Returns whether there is any active incognito window.
  bool IncognitoActive() const;
  // Returns whether there was any incognito window opened since an observer was
  // created. Returns true if |num_prev_incognito_opened|, which is passed by
  // the calling observer, is less than |num_incognito_window_opened_| of the
  // monitor.
  bool IncognitoLaunched(uint64_t num_prev_incognito_opened) const;

  // BrowserListObserver implementation.
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  // For testing.
  int num_active_incognito_windows() const {
    return num_active_incognito_windows_;
  }
  uint64_t num_incognito_window_opened() const {
    return num_incognito_window_opened_;
  }

 private:
  mutable base::Lock lock_;

  // The number of active incognito window(s).
  int num_active_incognito_windows_;
  // The number of incognito windows we have ever seen.
  uint64_t num_incognito_window_opened_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(WindowedIncognitoMonitor);
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_PERF_WINDOWED_INCOGNITO_OBSERVER_H_
