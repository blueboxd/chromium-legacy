// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_shift_tracker.h"

#include "cc/layers/heads_up_display_layer.h"
#include "cc/layers/picture_layer.h"
#include "third_party/blink/public/platform/web_pointer_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/location.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

static constexpr base::TimeDelta kTimerDelay =
    base::TimeDelta::FromMilliseconds(500);
static const float kRegionGranularitySteps = 60.0;
// TODO: Vary by Finch experiment parameter.
static const float kSweepLineRegionGranularity = 1.0;
static const float kMovementThreshold = 3.0;  // CSS pixels.

static FloatPoint LogicalStart(const FloatRect& rect,
                               const LayoutObject& object) {
  const ComputedStyle* style = object.Style();
  DCHECK(style);
  auto logical =
      PhysicalToLogical<float>(style->GetWritingMode(), style->Direction(),
                               rect.Y(), rect.MaxX(), rect.MaxY(), rect.X());
  return FloatPoint(logical.InlineStart(), logical.BlockStart());
}

static float GetMoveDistance(const FloatRect& old_rect,
                             const FloatRect& new_rect,
                             const LayoutObject& object) {
  FloatSize location_delta =
      LogicalStart(new_rect, object) - LogicalStart(old_rect, object);
  return std::max(fabs(location_delta.Width()), fabs(location_delta.Height()));
}

float LayoutShiftTracker::RegionGranularityScale(
    const IntRect& viewport) const {
  if (RuntimeEnabledFeatures::JankTrackingSweepLineEnabled())
    return kSweepLineRegionGranularity;

  return kRegionGranularitySteps /
         std::min(viewport.Height(), viewport.Width());
}

static bool EqualWithinMovementThreshold(const FloatPoint& a,
                                         const FloatPoint& b,
                                         const LayoutObject& object) {
  float threshold_physical_px =
      kMovementThreshold * object.StyleRef().EffectiveZoom();
  return fabs(a.X() - b.X()) < threshold_physical_px &&
         fabs(a.Y() - b.Y()) < threshold_physical_px;
}

static bool SmallerThanRegionGranularity(const FloatRect& rect,
                                         float granularity_scale) {
  return rect.Width() * granularity_scale < 0.5 ||
         rect.Height() * granularity_scale < 0.5;
}

static const PropertyTreeState PropertyTreeStateFor(
    const LayoutObject& object) {
  return object.FirstFragment().LocalBorderBoxProperties();
}

static void RegionToTracedValue(const Region& region,
                                double granularity_scale,
                                TracedValue& value) {
  value.BeginArray("region_rects");
  for (const IntRect& rect : region.Rects()) {
    value.BeginArray();
    value.PushInteger(clampTo<int>(roundf(rect.X() / granularity_scale)));
    value.PushInteger(clampTo<int>(roundf(rect.Y() / granularity_scale)));
    value.PushInteger(clampTo<int>(roundf(rect.Width() / granularity_scale)));
    value.PushInteger(clampTo<int>(roundf(rect.Height() / granularity_scale)));
    value.EndArray();
  }
  value.EndArray();
}

static void RegionToTracedValue(const LayoutShiftRegion& region,
                                double granularity_scale,
                                TracedValue& value) {
  Region old_region;
  for (IntRect rect : region.GetRects())
    old_region.Unite(Region(rect));
  RegionToTracedValue(old_region, granularity_scale, value);
}

#if DCHECK_IS_ON()
static bool ShouldLog(const LocalFrame& frame) {
  const String& url = frame.GetDocument()->Url().GetString();
  return !url.StartsWith("chrome-devtools:") && !url.StartsWith("devtools:");
}
#endif

LayoutShiftTracker::LayoutShiftTracker(LocalFrameView* frame_view)
    : frame_view_(frame_view),
      score_(0.0),
      weighted_score_(0.0),
      timer_(frame_view->GetFrame().GetTaskRunner(TaskType::kInternalDefault),
             this,
             &LayoutShiftTracker::TimerFired),
      frame_max_distance_(0.0),
      overall_max_distance_(0.0),
      observed_input_or_scroll_(false),
      most_recent_input_timestamp_initialized_(false) {}

void LayoutShiftTracker::ObjectShifted(
    const LayoutObject& source,
    const PropertyTreeState& property_tree_state,
    FloatRect old_rect,
    FloatRect new_rect) {
  if (old_rect.IsEmpty() || new_rect.IsEmpty())
    return;

  if (EqualWithinMovementThreshold(LogicalStart(old_rect, source),
                                   LogicalStart(new_rect, source), source))
    return;

  IntRect viewport =
      IntRect(IntPoint(),
              frame_view_->GetScrollableArea()->VisibleContentRect().Size());
  float scale = RegionGranularityScale(viewport);

  if (SmallerThanRegionGranularity(old_rect, scale) &&
      SmallerThanRegionGranularity(new_rect, scale))
    return;

  // Ignore layout objects that move (in the coordinate space of the paint
  // invalidation container) on scroll.
  // TODO(skobes): Find a way to detect when these objects shift.
  if (source.IsFixedPositioned() || source.IsStickyPositioned())
    return;

  // SVG elements don't participate in the normal layout algorithms and are
  // more likely to be used for animations.
  if (source.IsSVG())
    return;

  const auto root_state = PropertyTreeStateFor(*source.View());

  FloatClipRect clip_rect =
      GeometryMapper::LocalToAncestorClipRect(property_tree_state, root_state);

  // If the clip region is empty, then the resulting layout shift isn't visible
  // in the viewport so ignore it.
  if (!clip_rect.IsInfinite() && clip_rect.Rect().IsEmpty())
    return;

  GeometryMapper::SourceToDestinationRect(property_tree_state.Transform(),
                                          root_state.Transform(), old_rect);
  GeometryMapper::SourceToDestinationRect(property_tree_state.Transform(),
                                          root_state.Transform(), new_rect);

  if (EqualWithinMovementThreshold(old_rect.Location(), new_rect.Location(),
                                   source)) {
    return;
  }

  if (EqualWithinMovementThreshold(old_rect.Location() + frame_scroll_delta_,
                                   new_rect.Location(), source)) {
    // TODO(skobes): Checking frame_scroll_delta_ is an imperfect solution to
    // allowing counterscrolled layout shifts. Ideally, we would map old_rect
    // to viewport coordinates using the previous frame's scroll tree.
    return;
  }

  FloatRect clipped_old_rect(old_rect), clipped_new_rect(new_rect);
  if (!clip_rect.IsInfinite()) {
    clipped_old_rect.Intersect(clip_rect.Rect());
    clipped_new_rect.Intersect(clip_rect.Rect());
  }

  IntRect visible_old_rect = RoundedIntRect(clipped_old_rect);
  visible_old_rect.Intersect(viewport);
  IntRect visible_new_rect = RoundedIntRect(clipped_new_rect);
  visible_new_rect.Intersect(viewport);

  if (visible_old_rect.IsEmpty() && visible_new_rect.IsEmpty())
    return;

  // Compute move distance based on unclipped rects, to accurately determine how
  // much the element moved.
  float move_distance = GetMoveDistance(old_rect, new_rect, source);
  frame_max_distance_ = std::max(frame_max_distance_, move_distance);

#if DCHECK_IS_ON()
  LocalFrame& frame = frame_view_->GetFrame();
  if (ShouldLog(frame)) {
    DVLOG(2) << "in " << (frame.IsMainFrame() ? "" : "subframe ")
             << frame.GetDocument()->Url().GetString() << ", "
             << source.DebugName() << " moved from " << old_rect.ToString()
             << " to " << new_rect.ToString() << " (visible from "
             << visible_old_rect.ToString() << " to "
             << visible_new_rect.ToString() << ")";
  }
#endif

  visible_old_rect.Scale(scale);
  visible_new_rect.Scale(scale);

  if (RuntimeEnabledFeatures::JankTrackingSweepLineEnabled()) {
    region_experimental_.AddRect(visible_old_rect);
    region_experimental_.AddRect(visible_new_rect);
  } else {
    region_.Unite(Region(visible_old_rect));
    region_.Unite(Region(visible_new_rect));
  }
}

void LayoutShiftTracker::NotifyObjectPrePaint(
    const LayoutObject& object,
    const PropertyTreeState& property_tree_state,
    const IntRect& old_visual_rect,
    const IntRect& new_visual_rect) {
  if (!IsActive())
    return;

  ObjectShifted(object, property_tree_state, FloatRect(old_visual_rect),
                FloatRect(new_visual_rect));
}

void LayoutShiftTracker::NotifyCompositedLayerMoved(
    const LayoutObject& layout_object,
    FloatRect old_layer_rect,
    FloatRect new_layer_rect) {
  if (!IsActive())
    return;

  // Make sure we can access a transform node.
  if (!layout_object.FirstFragment().HasLocalBorderBoxProperties())
    return;

  ObjectShifted(layout_object, PropertyTreeStateFor(layout_object),
                old_layer_rect, new_layer_rect);
}

double LayoutShiftTracker::SubframeWeightingFactor() const {
  LocalFrame& frame = frame_view_->GetFrame();
  if (frame.IsMainFrame())
    return 1;

  // Map the subframe view rect into the coordinate space of the local root.
  FloatClipRect subframe_cliprect =
      FloatClipRect(FloatRect(FloatPoint(), FloatSize(frame_view_->Size())));
  GeometryMapper::LocalToAncestorVisualRect(
      frame_view_->GetLayoutView()->FirstFragment().LocalBorderBoxProperties(),
      PropertyTreeState::Root(), subframe_cliprect);
  auto subframe_rect = PhysicalRect::EnclosingRect(subframe_cliprect.Rect());

  // Intersect with the portion of the local root that overlaps the main frame.
  frame.LocalFrameRoot().View()->MapToVisualRectInTopFrameSpace(subframe_rect);
  IntSize subframe_visible_size = subframe_rect.PixelSnappedSize();
  IntSize main_frame_size = frame.GetPage()->GetVisualViewport().Size();

  // TODO(crbug.com/940711): This comparison ignores page scale and CSS
  // transforms above the local root.
  return static_cast<double>(subframe_visible_size.Area()) /
         main_frame_size.Area();
}

void LayoutShiftTracker::NotifyPrePaintFinished() {
  if (!IsActive())
    return;
  bool use_sweep_line = RuntimeEnabledFeatures::JankTrackingSweepLineEnabled();
  bool region_is_empty =
      use_sweep_line ? region_experimental_.IsEmpty() : region_.IsEmpty();
  if (region_is_empty)
    return;

  IntRect viewport = frame_view_->GetScrollableArea()->VisibleContentRect();
  if (viewport.IsEmpty())
    return;

  double granularity_scale = RegionGranularityScale(viewport);
  IntRect scaled_viewport = viewport;
  scaled_viewport.Scale(granularity_scale);

  double viewport_area =
      double(scaled_viewport.Width()) * double(scaled_viewport.Height());
  uint64_t region_area =
      use_sweep_line ? region_experimental_.Area() : region_.Area();
  double impact_fraction = region_area / viewport_area;
  DCHECK_GT(impact_fraction, 0);

  DCHECK_GT(frame_max_distance_, 0.0);
  double viewport_max_dimension = std::max(viewport.Width(), viewport.Height());
  double move_distance_factor =
      (frame_max_distance_ < viewport_max_dimension)
          ? double(frame_max_distance_) / viewport_max_dimension
          : 1.0;
  double score_delta = impact_fraction * move_distance_factor;
  double weighted_score_delta = score_delta * SubframeWeightingFactor();

  overall_max_distance_ = std::max(overall_max_distance_, frame_max_distance_);

#if DCHECK_IS_ON()
  LocalFrame& frame = frame_view_->GetFrame();
  if (ShouldLog(frame)) {
    DVLOG(1) << "in " << (frame.IsMainFrame() ? "" : "subframe ")
             << frame.GetDocument()->Url().GetString() << ", viewport was "
             << (impact_fraction * 100) << "% impacted with distance fraction "
             << move_distance_factor;
  }
#endif

  if (pointerdown_pending_data_.saw_pointerdown) {
    pointerdown_pending_data_.score_delta += score_delta;
    pointerdown_pending_data_.weighted_score_delta += weighted_score_delta;
  } else {
    ReportShift(score_delta, weighted_score_delta);
  }

  if (use_sweep_line) {
    if (!region_experimental_.IsEmpty()) {
      SetLayoutShiftRects(region_experimental_.GetRects(), 1, true);
    }
    region_experimental_.Reset();
  } else {
    if (!region_.IsEmpty()) {
      SetLayoutShiftRects(region_.Rects(), granularity_scale, false);
    }
    region_ = Region();
  }
  frame_max_distance_ = 0.0;
  frame_scroll_delta_ = ScrollOffset();
}

void LayoutShiftTracker::ReportShift(double score_delta,
                                     double weighted_score_delta) {
  LocalFrame& frame = frame_view_->GetFrame();
  bool had_recent_input = timer_.IsActive();

  if (!had_recent_input) {
    score_ += score_delta;
    if (weighted_score_delta > 0) {
      weighted_score_ += weighted_score_delta;
      frame.Client()->DidObserveLayoutShift(weighted_score_delta,
                                            observed_input_or_scroll_);
    }
  }

  if (RuntimeEnabledFeatures::LayoutInstabilityAPIEnabled(
          frame.GetDocument()) &&
      frame.DomWindow()) {
    WindowPerformance* performance =
        DOMWindowPerformance::performance(*frame.DomWindow());
    if (performance) {
      performance->AddLayoutJankFraction(score_delta, had_recent_input,
                                         most_recent_input_timestamp_);
    }
  }

  TRACE_EVENT_INSTANT2("loading", "LayoutShift", TRACE_EVENT_SCOPE_THREAD,
                       "data", PerFrameTraceData(score_delta, had_recent_input),
                       "frame", ToTraceValue(&frame));

#if DCHECK_IS_ON()
  if (ShouldLog(frame)) {
    DVLOG(1) << "in " << (frame.IsMainFrame() ? "" : "subframe ")
             << frame.GetDocument()->Url().GetString() << ", layout shift of "
             << score_delta
             << (had_recent_input ? " excluded by recent input" : " reported")
             << "; cumulative score is " << score_;
  }
#endif
}

void LayoutShiftTracker::NotifyInput(const WebInputEvent& event) {
  const WebInputEvent::Type type = event.GetType();
  const bool saw_pointerdown = pointerdown_pending_data_.saw_pointerdown;
  const bool pointerdown_became_tap =
      saw_pointerdown && type == WebInputEvent::kPointerUp;
  const bool event_type_stops_pointerdown_buffering =
      type == WebInputEvent::kPointerUp ||
      type == WebInputEvent::kPointerCausedUaAction ||
      type == WebInputEvent::kPointerCancel;

  // Only non-hovering pointerdown requires buffering.
  const bool is_hovering_pointerdown =
      type == WebInputEvent::kPointerDown &&
      static_cast<const WebPointerEvent&>(event).hovering;

  const bool should_trigger_shift_exclusion =
      type == WebInputEvent::kMouseDown || type == WebInputEvent::kKeyDown ||
      type == WebInputEvent::kRawKeyDown ||
      // We need to explicitly include tap, as if there are no listeners, we
      // won't receive the pointer events.
      type == WebInputEvent::kGestureTap || is_hovering_pointerdown ||
      pointerdown_became_tap;

  if (should_trigger_shift_exclusion) {
    observed_input_or_scroll_ = true;

    // This cancels any previously scheduled task from the same timer.
    timer_.StartOneShot(kTimerDelay, FROM_HERE);
    UpdateInputTimestamp(event.TimeStamp());
  }

  if (saw_pointerdown && event_type_stops_pointerdown_buffering) {
    double score_delta = pointerdown_pending_data_.score_delta;
    if (score_delta > 0)
      ReportShift(score_delta, pointerdown_pending_data_.weighted_score_delta);
    pointerdown_pending_data_ = PointerdownPendingData();
  }
  if (type == WebInputEvent::kPointerDown && !is_hovering_pointerdown)
    pointerdown_pending_data_.saw_pointerdown = true;
}

void LayoutShiftTracker::UpdateInputTimestamp(base::TimeTicks timestamp) {
  if (!most_recent_input_timestamp_initialized_) {
    most_recent_input_timestamp_ = timestamp;
    most_recent_input_timestamp_initialized_ = true;
  } else if (timestamp > most_recent_input_timestamp_) {
    most_recent_input_timestamp_ = timestamp;
  }
}

void LayoutShiftTracker::NotifyScroll(ScrollType scroll_type,
                                      ScrollOffset delta) {
  frame_scroll_delta_ += delta;

  // Only set observed_input_or_scroll_ for user-initiated scrolls, and not
  // other scrolls such as hash fragment navigations.
  if (scroll_type == kUserScroll || scroll_type == kCompositorScroll)
    observed_input_or_scroll_ = true;
}

void LayoutShiftTracker::NotifyViewportSizeChanged() {
  // This cancels any previously scheduled task from the same timer.
  timer_.StartOneShot(kTimerDelay, FROM_HERE);
  UpdateInputTimestamp(base::TimeTicks::Now());
}

bool LayoutShiftTracker::IsActive() {
  // This eliminates noise from the private Page object created by
  // SVGImage::DataChanged.
  if (frame_view_->GetFrame().GetChromeClient().IsSVGImageChromeClient())
    return false;
  return true;
}

std::unique_ptr<TracedValue> LayoutShiftTracker::PerFrameTraceData(
    double score_delta,
    bool input_detected) const {
  auto value = std::make_unique<TracedValue>();
  value->SetDouble("score", score_delta);
  value->SetDouble("cumulative_score", score_);
  value->SetDouble("overall_max_distance", overall_max_distance_);
  value->SetDouble("frame_max_distance", frame_max_distance_);

  float granularity_scale = RegionGranularityScale(
      IntRect(IntPoint(),
              frame_view_->GetScrollableArea()->VisibleContentRect().Size()));
  if (RuntimeEnabledFeatures::JankTrackingSweepLineEnabled())
    RegionToTracedValue(region_experimental_, granularity_scale, *value);
  else
    RegionToTracedValue(region_, granularity_scale, *value);

  value->SetBoolean("is_main_frame", frame_view_->GetFrame().IsMainFrame());
  value->SetBoolean("had_recent_input", input_detected);
  return value;
}

WebVector<gfx::Rect> LayoutShiftTracker::ConvertIntRectsToGfxRects(
    const Vector<IntRect>& int_rects,
    double granularity_scale) {
  WebVector<gfx::Rect> rects;
  for (const IntRect& rect : int_rects) {
    gfx::Rect r = gfx::Rect(
        rect.X() / granularity_scale, rect.Y() / granularity_scale,
        rect.Width() / granularity_scale, rect.Height() / granularity_scale);
    rects.emplace_back(r);
  }
  return rects;
}

void LayoutShiftTracker::SetLayoutShiftRects(const Vector<IntRect>& int_rects,
                                             double granularity_scale,
                                             bool using_sweep_line) {
  // Store the layout shift rects in the HUD layer.
  GraphicsLayer* root_graphics_layer =
      frame_view_->GetLayoutView()->Compositor()->RootGraphicsLayer();
  if (!root_graphics_layer)
    return;

  cc::Layer* cc_layer = root_graphics_layer->CcLayer();
  if (!cc_layer)
    return;
  if (cc_layer->layer_tree_host()) {
    if (!cc_layer->layer_tree_host()->GetDebugState().show_layout_shift_regions)
      return;
    if (cc_layer->layer_tree_host()->hud_layer()) {
      WebVector<gfx::Rect> rects;
      if (using_sweep_line) {
        Region old_region;
        for (IntRect rect : int_rects)
          old_region.Unite(Region(rect));
        rects =
            ConvertIntRectsToGfxRects(old_region.Rects(), granularity_scale);
      } else {
        rects = ConvertIntRectsToGfxRects(int_rects, granularity_scale);
      }
      cc_layer->layer_tree_host()->hud_layer()->SetLayoutShiftRects(
          rects.ReleaseVector());
      cc_layer->layer_tree_host()->hud_layer()->SetNeedsPushProperties();
    }
  }
}

}  // namespace blink
