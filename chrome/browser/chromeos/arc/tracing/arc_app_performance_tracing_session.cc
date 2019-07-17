// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/tracing/arc_app_performance_tracing_session.h"

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/arc/tracing/arc_app_performance_tracing.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "content/public/browser/browser_thread.h"
#include "ui/aura/window.h"

namespace arc {

namespace {

// Defines the delay to start tracing after ARC++ window gets activated.
// This is done to avoid likely redundant statistics collection during the app
// initialization/loading time.
constexpr base::TimeDelta kInitTracingDelay = base::TimeDelta::FromMinutes(1);

// Defines the delay to start next session of capturing statistics for the same
// active app or in case the app was already reported.
constexpr base::TimeDelta kNextTracingDelay = base::TimeDelta::FromMinutes(20);

// Target FPS, all reference devices has 60 FPS.
// TODO(khmel), detect this per device.
constexpr uint64_t kTargetFps = 60;

constexpr base::TimeDelta kTargetFrameTime =
    base::TimeDelta::FromSeconds(1) / kTargetFps;

// Used for detection the idle. App considered in idle state when there is no
// any commit for |kIdleThresholdFrames| frames.
constexpr uint64_t kIdleThresholdFrames = 10;

std::string GetHistogramName(const std::string& category,
                             const std::string& name) {
  return base::StringPrintf("Arc.Runtime.Performance.%s.%s", name.c_str(),
                            category.c_str());
}

void ReportFPS(const std::string& category_name, double fps) {
  DCHECK(!category_name.empty());
  DCHECK_GT(fps, 0);
  base::UmaHistogramCounts100(GetHistogramName(category_name, "FPS"),
                              static_cast<int>(std::round(fps)));
}

void ReportCommitError(const std::string& category_name, double error_mcs) {
  DCHECK(!category_name.empty());
  DCHECK_GE(error_mcs, 0);
  base::UmaHistogramCustomCounts(
      GetHistogramName(category_name, "CommitDeviation"),
      static_cast<int>(std::round(error_mcs)), 100 /* min */, 5000 /* max */,
      50 /* buckets */);
}

void ReportQuality(const std::string& category_name, double quality) {
  DCHECK(!category_name.empty());
  DCHECK_GT(quality, 0);
  // Report quality from 0 to 100%.
  const int sample = (int)(quality * 100.0);
  base::UmaHistogramPercentage(GetHistogramName(category_name, "RenderQuality"),
                               sample);
}

}  // namespace

ArcAppPerformanceTracingSession::ArcAppPerformanceTracingSession(
    ArcAppPerformanceTracing* owner,
    aura::Window* window,
    const std::string& category,
    const base::TimeDelta& tracing_period)
    : owner_(owner),
      window_(window),
      category_(category),
      tracing_period_(tracing_period) {}

ArcAppPerformanceTracingSession::~ArcAppPerformanceTracingSession() {
  // Discard any active tracing if any.
  Stop();
}

void ArcAppPerformanceTracingSession::Schedule() {
  DCHECK(!tracing_active_);
  DCHECK(!tracing_timer_.IsRunning());
  const base::TimeDelta delay =
      owner_->WasReported(category_) ? kNextTracingDelay : kInitTracingDelay;
  tracing_timer_.Start(FROM_HERE, delay,
                       base::BindOnce(&ArcAppPerformanceTracingSession::Start,
                                      base::Unretained(this)));
}

void ArcAppPerformanceTracingSession::OnSurfaceDestroying(
    exo::Surface* surface) {
  Stop();
}

void ArcAppPerformanceTracingSession::OnCommit(exo::Surface* surface) {
  HandleCommit(base::Time::Now());
}

void ArcAppPerformanceTracingSession::FireTimerForTesting() {
  tracing_timer_.FireNow();
}

void ArcAppPerformanceTracingSession::OnCommitForTesting(
    const base::Time& timestamp) {
  HandleCommit(timestamp);
}

void ArcAppPerformanceTracingSession::Start() {
  DCHECK(!tracing_timer_.IsRunning());

  VLOG(1) << "Start tracing for the category " << category_ << ".";

  frame_deltas_.clear();
  last_commit_timestamp_ = base::Time();

  exo::Surface* const surface = exo::GetShellMainSurface(window_);
  DCHECK(surface);
  surface->AddSurfaceObserver(this);

  // Schedule result analyzing at the end of tracing.
  tracing_timer_.Start(FROM_HERE, tracing_period_,
                       base::BindOnce(&ArcAppPerformanceTracingSession::Analyze,
                                      base::Unretained(this)));
  tracing_active_ = true;
}

void ArcAppPerformanceTracingSession::Stop() {
  tracing_active_ = false;
  tracing_timer_.Stop();
  exo::Surface* const surface = exo::GetShellMainSurface(window_);
  // Surface might be destroyed.
  if (surface)
    surface->RemoveSurfaceObserver(this);
}

void ArcAppPerformanceTracingSession::HandleCommit(
    const base::Time& timestamp) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (last_commit_timestamp_.is_null()) {
    last_commit_timestamp_ = timestamp;
    return;
  }

  const base::TimeDelta frame_delta = timestamp - last_commit_timestamp_;
  last_commit_timestamp_ = timestamp;

  const uint64_t display_frames_passed =
      (frame_delta + kTargetFrameTime / 2) / kTargetFrameTime;
  if (display_frames_passed >= kIdleThresholdFrames) {
    // Idle is detected, try the next time.
    Stop();
    Schedule();
    return;
  }
  frame_deltas_.emplace_back(frame_delta);
}

void ArcAppPerformanceTracingSession::Analyze() {
  // No more data is needed, stop active tracing.
  Stop();

  // Check last commit timestamp if we are in idle at this moment.
  const base::TimeDelta last_frame_delta =
      base::Time::Now() - last_commit_timestamp_;
  if (last_frame_delta >= kTargetFrameTime * kIdleThresholdFrames) {
    // Current idle state is detected, try next time.
    Schedule();
    return;
  }

  VLOG(1) << "Analyze tracing for the category " << category_ << ".";

  DCHECK(!frame_deltas_.empty());

  double vsync_error_deviation_accumulator = 0;
  for (const auto& frame_delta : frame_deltas_) {
    // Calculate the number of display frames passed between two updates.
    // Ideally we should have one frame for target FPS. In case the app drops
    // frames, the number of dropped frames would be accounted. The result is
    // fractional part of target frame interval |kTargetFrameTime| and is less
    // or equal half of it.
    const uint64_t display_frames_passed =
        (frame_delta + kTargetFrameTime / 2) / kTargetFrameTime;
    // Calculate difference from the ideal commit time, that should happen with
    // equal delay for each display frame.
    const base::TimeDelta vsync_error =
        frame_delta - display_frames_passed * kTargetFrameTime;
    vsync_error_deviation_accumulator +=
        (vsync_error.InMicrosecondsF() * vsync_error.InMicrosecondsF());
  }
  const double vsync_error =
      sqrt(vsync_error_deviation_accumulator / frame_deltas_.size());

  std::sort(frame_deltas_.begin(), frame_deltas_.end());
  // Get 10% and 90% indices.
  const size_t lower_position = frame_deltas_.size() / 10;
  const size_t upper_position = frame_deltas_.size() - 1 - lower_position;
  const double quality = frame_deltas_[lower_position].InMicrosecondsF() /
                         frame_deltas_[upper_position].InMicrosecondsF();

  const double fps = frame_deltas_.size() / tracing_period_.InSecondsF();
  VLOG(1) << "Analyzing is done for " << category_ << " "
          << " FPS: " << fps << ", quality: " << quality
          << ", vsync_error: " << vsync_error;

  ReportFPS(category_, fps);
  ReportCommitError(category_, vsync_error);
  ReportQuality(category_, quality);

  owner_->SetReported(category_);

  // Reschedule the next tracing session.
  Schedule();
}

}  // namespace arc
