// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/frame_rate_decider.h"

#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_manager.h"

namespace viz {

FrameRateDecider::ScopedAggregate::ScopedAggregate(FrameRateDecider* decider)
    : decider_(decider) {
  decider_->StartAggregation();
}

FrameRateDecider::ScopedAggregate::~ScopedAggregate() {
  decider_->EndAggregation();
}

FrameRateDecider::FrameRateDecider(SurfaceManager* surface_manager,
                                   Client* client,
                                   bool using_synthetic_bfs)
    : supported_intervals_{BeginFrameArgs::DefaultInterval()},
      surface_manager_(surface_manager),
      client_(client),
      using_synthetic_bfs_(using_synthetic_bfs) {
  surface_manager_->AddObserver(this);
}

FrameRateDecider::~FrameRateDecider() {
  surface_manager_->RemoveObserver(this);
}

void FrameRateDecider::SetSupportedFrameIntervals(
    std::vector<base::TimeDelta> supported_intervals) {
  DCHECK(!inside_surface_aggregation_);

  supported_intervals_ = std::move(supported_intervals);
  std::sort(supported_intervals_.begin(), supported_intervals_.end());
  UpdatePreferredFrameIntervalIfNeeded();
}

void FrameRateDecider::OnSurfaceWillBeDrawn(Surface* surface) {
  // If there are multiple displays, we receive callbacks when a surface is
  // drawn on any of these displays. Ensure that we only update the internal
  // tracking when the Display corresponding to this decider is drawing.
  if (!inside_surface_aggregation_)
    return;

  if (!multiple_refresh_rates_supported())
    return;

  // Update the list of surfaces drawn in this frame along with the currently
  // active CompositorFrame. We use the list from the previous frame to track
  // which surfaces were updated in this display draw.
  const SurfaceId& surface_id = surface->surface_id();
  const uint64_t active_index = surface->GetActiveFrameIndex();

  auto it = current_surface_id_to_active_index_.find(surface_id);
  if (it == current_surface_id_to_active_index_.end()) {
    current_surface_id_to_active_index_[surface_id] = active_index;
  } else {
    DCHECK_EQ(it->second, active_index)
        << "Same display frame should not draw a surface with different "
           "CompositorFrames";
  }

  it = prev_surface_id_to_active_index_.find(surface_id);
  if (it == prev_surface_id_to_active_index_.end() ||
      it->second != active_index) {
    frame_sinks_updated_in_previous_frame_.insert(surface_id.frame_sink_id());
  }
  frame_sinks_drawn_in_previous_frame_.insert(surface_id.frame_sink_id());
}

void FrameRateDecider::StartAggregation() {
  DCHECK(!inside_surface_aggregation_);

  inside_surface_aggregation_ = true;
  frame_sinks_updated_in_previous_frame_.clear();
  frame_sinks_drawn_in_previous_frame_.clear();
}

void FrameRateDecider::EndAggregation() {
  DCHECK(inside_surface_aggregation_);

  inside_surface_aggregation_ = false;
  prev_surface_id_to_active_index_.swap(current_surface_id_to_active_index_);
  current_surface_id_to_active_index_.clear();

  UpdatePreferredFrameIntervalIfNeeded();
}

void FrameRateDecider::UpdatePreferredFrameIntervalIfNeeded() {
  if (!multiple_refresh_rates_supported())
    return;

  // If lowering the refresh rate is supported by the platform then we do this
  // in all cases where the content drawing onscreen animates at a fixed rate.
  // This allows the platform to refresh the screen at a lower rate which is
  // power efficient.
  //
  // But if we're using a synthetic begin frame source, then there is no benefit
  // in ticking at a lower rate unless there are multiple frame sinks animating
  // at a fixed rate. Ticking at a lower rate in this case ensures that updates
  // from the frame sinks are aligned to the same vsync, allowing the compositor
  // to draw at a lower rate.
  if (using_synthetic_bfs_) {
    int num_of_frame_sinks_with_fixed_interval = 0;
    for (const auto& frame_sink_id : frame_sinks_drawn_in_previous_frame_) {
      if (client_->GetPreferredFrameIntervalForFrameSinkId(frame_sink_id) !=
          BeginFrameArgs::MinInterval())
        num_of_frame_sinks_with_fixed_interval++;
    }

    if (num_of_frame_sinks_with_fixed_interval < 2) {
      TRACE_EVENT_INSTANT0(
          "viz",
          "FrameRateDecider::UpdatePreferredFrameIntervalIfNeeded - not enough "
          "frame sinks to toggle",
          TRACE_EVENT_SCOPE_THREAD);
      SetPreferredInterval(UnspecifiedFrameInterval());
      return;
    }
  }

  // The code below picks the optimal frame interval for the display based on
  // the frame sinks which were updated in this frame. This is because we want
  // the display's update rate to be decided based on onscreen content that is
  // animating. This ensures that, for instance, if we're currently displaying
  // a video while the rest of the page is static, we choose the frame interval
  // optimal for the video.
  base::TimeDelta min_frame_sink_interval =
      frame_sinks_updated_in_previous_frame_.empty()
          ? BeginFrameArgs::MinInterval()
          : base::TimeDelta::Max();
  for (const auto& frame_sink_id : frame_sinks_updated_in_previous_frame_) {
    min_frame_sink_interval = std::min(
        min_frame_sink_interval,
        client_->GetPreferredFrameIntervalForFrameSinkId(frame_sink_id));
  }
  TRACE_EVENT_INSTANT1("viz",
                       "FrameRateDecider::UpdatePreferredFrameIntervalIfNeeded",
                       TRACE_EVENT_SCOPE_THREAD, "min_frame_sink_interval",
                       min_frame_sink_interval.InMillisecondsF());

  // If we don't have an explicit preference from the active frame sinks, then
  // we use a 0 value for preferred frame interval to let the framework pick the
  // ideal refresh rate.
  base::TimeDelta new_preferred_interval = UnspecifiedFrameInterval();
  if (min_frame_sink_interval != BeginFrameArgs::MinInterval()) {
    for (auto supported_interval : supported_intervals_) {
      // Pick the display interval which is closest to the preferred interval.
      if ((min_frame_sink_interval - supported_interval).magnitude() <
          (min_frame_sink_interval - new_preferred_interval).magnitude()) {
        new_preferred_interval = supported_interval;
      }
    }
  }

  SetPreferredInterval(new_preferred_interval);
}

void FrameRateDecider::SetPreferredInterval(
    base::TimeDelta new_preferred_interval) {
  TRACE_EVENT_INSTANT1("viz", "FrameRateDecider::SetPreferredInterval",
                       TRACE_EVENT_SCOPE_THREAD, "new_preferred_interval",
                       new_preferred_interval.InMillisecondsF());

  if (new_preferred_interval == last_computed_preferred_frame_interval_) {
    num_of_frames_since_preferred_interval_changed_++;
  } else {
    num_of_frames_since_preferred_interval_changed_ = 0u;
  }
  last_computed_preferred_frame_interval_ = new_preferred_interval;

  if (current_preferred_frame_interval_ == new_preferred_interval)
    return;

  // The min num of frames heuristic is to ensure we see a constant pattern
  // before toggling the global setting to avoid unnecessary switches when
  // lowering the refresh rate. For increasing the refresh rate we toggle
  // immediately to prioritize smoothness.
  bool should_toggle =
      current_preferred_frame_interval_ > new_preferred_interval ||
      num_of_frames_since_preferred_interval_changed_ >=
          min_num_of_frames_to_toggle_interval_;
  if (should_toggle) {
    current_preferred_frame_interval_ = new_preferred_interval;
    client_->SetPreferredFrameInterval(new_preferred_interval);
  }
}

}  // namespace viz
