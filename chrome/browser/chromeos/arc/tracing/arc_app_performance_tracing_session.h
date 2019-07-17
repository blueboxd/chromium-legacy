// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_APP_PERFORMANCE_TRACING_SESSION_H_
#define CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_APP_PERFORMANCE_TRACING_SESSION_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/exo/surface_observer.h"

namespace aura {
class Window;
}  // namespace aura

namespace exo {
class Surface;
}  // namespace exo

namespace arc {

class ArcAppPerformanceTracing;

// Implements Surface commit tracing for the target window.
class ArcAppPerformanceTracingSession : public exo::SurfaceObserver {
 public:
  ArcAppPerformanceTracingSession(ArcAppPerformanceTracing* owner,
                                  aura::Window* window,
                                  const std::string& category,
                                  const base::TimeDelta& tracing_period);
  ~ArcAppPerformanceTracingSession() override;

  // Schedules tracing with a delay based on condition if tracing category was
  // previously reported or not. Creator of |ArcAppPerformanceTracingSession| is
  // responsible for the initial scheduling. This can internally re-scheduled
  // during the life-cycle of the tracing session.
  void Schedule();

  // exo::SurfaceObserver:
  void OnSurfaceDestroying(exo::Surface* surface) override;
  void OnCommit(exo::Surface* surface) override;

  void FireTimerForTesting();
  void OnCommitForTesting(const base::Time& timestamp);

  bool tracing_active() const { return tracing_active_; }
  const aura::Window* window() const { return window_; }

 private:
  // Starts tracing by observing commits to the |exo::Surface| attached to the
  // current |window_|.
  void Start();

  // Stops tracing for the current |window_|.
  void Stop();

  // Handles the next commit update. This is unified handler for testing and
  // production code.
  void HandleCommit(const base::Time& timestamp);

  // Stops current tracing, analyzes captured tracing results and schedules the
  // next tracing for the current |window_|.
  void Analyze();

  // Unowned pointer.
  ArcAppPerformanceTracing* const owner_;
  aura::Window* const window_;

  // Current tracing category.
  const std::string category_;

  // Timer to start Surface commit tracing delayed.
  base::OneShotTimer tracing_timer_;

  // Period for tracing sessions.
  const base::TimeDelta tracing_period_;

  // Timestamp of last commit event.
  base::Time last_commit_timestamp_;

  // Accumulator for commit deltas.
  std::vector<base::TimeDelta> frame_deltas_;

  // Indicates that tracing is in active state.
  bool tracing_active_ = false;

  DISALLOW_COPY_AND_ASSIGN(ArcAppPerformanceTracingSession);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_APP_PERFORMANCE_TRACING_SESSION_H_
