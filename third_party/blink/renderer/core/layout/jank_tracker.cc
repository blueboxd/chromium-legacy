// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/jank_tracker.h"

#include "base/metrics/field_trial_params.h"
#include "cc/layers/heads_up_display_layer.h"
#include "cc/layers/picture_layer.h"
#include "third_party/blink/public/common/features.h"
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

static constexpr TimeDelta kTimerDelay = TimeDelta::FromMilliseconds(500);
static const float kMovementThreshold = 3.0;  // CSS pixels.

// Level of granularity snapping for impact region (see RegionGranularityScale).
enum GranularitySnapping { kPixel = 1, kLow = 2, kMedium = 3, kHigh = 4 };

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

static float RegionGranularityScale(const IntRect& viewport) {
  static const base::FeatureParam<int> kSweepLineRegionGranularity{
      &blink::features::kJankTrackingSweepLine, "sweep_line_region_granularity",
      kHigh};

  GranularitySnapping snapping_type =
      RuntimeEnabledFeatures::JankTrackingSweepLineEnabled()
          ? (GranularitySnapping)kSweepLineRegionGranularity.Get()
          : kHigh;
  switch (snapping_type) {
    case kPixel:
      // Snap to multiples of 1px.
      return 1.0;
    case kLow:
      // Snap to multiples of 2px.
      return 1 / 2.0;
    case kMedium:
      // Snap to multiples of 5px.
      return 1 / 5.0;
    case kHigh:
      // Snap to multiples of 1/60th of min(viewport width, viewport height).
      return 60.0 / std::min(viewport.Height(), viewport.Width());
  }
  NOTREACHED();
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

static void RegionToTracedValue(const JankRegion& region,
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

JankTracker::JankTracker(LocalFrameView* frame_view)
    : frame_view_(frame_view),
      score_(0.0),
      score_with_move_distance_(0.0),
      weighted_score_(0.0),
      timer_(frame_view->GetFrame().GetTaskRunner(TaskType::kInternalDefault),
             this,
             &JankTracker::TimerFired),
      frame_max_distance_(0.0),
      overall_max_distance_(0.0),
      observed_input_or_scroll_(false) {}

void JankTracker::AccumulateJank(const LayoutObject& source,
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
  // TODO(skobes): Find a way to detect when these objects jank.
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

void JankTracker::NotifyObjectPrePaint(
    const LayoutObject& object,
    const PropertyTreeState& property_tree_state,
    const IntRect& old_visual_rect,
    const IntRect& new_visual_rect) {
  if (!IsActive())
    return;

  AccumulateJank(object, property_tree_state, FloatRect(old_visual_rect),
                 FloatRect(new_visual_rect));
}

void JankTracker::NotifyCompositedLayerMoved(const LayoutObject& layout_object,
                                             FloatRect old_layer_rect,
                                             FloatRect new_layer_rect) {
  if (!IsActive())
    return;

  // Make sure we can access a transform node.
  if (!layout_object.FirstFragment().HasLocalBorderBoxProperties())
    return;

  // Convert to the local transform space, whose origin is the layer's previous
  // location because the property trees haven't been updated yet.
  FloatPoint transform_parent_offset = -old_layer_rect.Location();
  old_layer_rect.MoveBy(transform_parent_offset);
  new_layer_rect.MoveBy(transform_parent_offset);

  AccumulateJank(layout_object, PropertyTreeStateFor(layout_object),
                 old_layer_rect, new_layer_rect);
}

double JankTracker::SubframeWeightingFactor() const {
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

  // TODO(crbug.com/939050): This does not update on window resize.
  IntSize main_frame_size = frame.GetPage()->GetVisualViewport().Size();

  // TODO(crbug.com/940711): This comparison ignores page scale and CSS
  // transforms above the local root.
  return static_cast<double>(subframe_visible_size.Area()) /
         main_frame_size.Area();
}

void JankTracker::NotifyPrePaintFinished() {
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
  double jank_fraction = region_area / viewport_area;
  DCHECK_GT(jank_fraction, 0);

  score_ += jank_fraction;

  DCHECK_GT(frame_max_distance_, 0.0);
  double viewport_max_dimension = std::max(viewport.Width(), viewport.Height());
  double move_distance_factor =
      (frame_max_distance_ < viewport_max_dimension)
          ? double(frame_max_distance_) / viewport_max_dimension
          : 1.0;
  double jank_fraction_with_move_distance =
      jank_fraction * move_distance_factor;

  score_with_move_distance_ += jank_fraction_with_move_distance;

  overall_max_distance_ = std::max(overall_max_distance_, frame_max_distance_);

  LocalFrame& frame = frame_view_->GetFrame();
#if DCHECK_IS_ON()
  if (ShouldLog(frame)) {
    DVLOG(1) << "in " << (frame.IsMainFrame() ? "" : "subframe ")
             << frame.GetDocument()->Url().GetString() << ", viewport was "
             << (jank_fraction * 100) << "% janked; raising score to "
             << score_;
  }
#endif

  TRACE_EVENT_INSTANT2(
      "loading", "LayoutShift", TRACE_EVENT_SCOPE_THREAD, "data",
      PerFrameTraceData(jank_fraction, jank_fraction_with_move_distance,
                        granularity_scale),
      "frame", ToTraceValue(&frame));

  double weighted_jank_fraction = jank_fraction * SubframeWeightingFactor();
  if (weighted_jank_fraction > 0) {
    weighted_score_ += weighted_jank_fraction;
    if (RuntimeEnabledFeatures::LayoutInstabilityMoveDistanceEnabled())
      weighted_jank_fraction *= move_distance_factor;
    frame.Client()->DidObserveLayoutJank(weighted_jank_fraction,
                                         observed_input_or_scroll_);
  }

  if (RuntimeEnabledFeatures::LayoutInstabilityAPIEnabled(
          frame.GetDocument()) &&
      frame.DomWindow()) {
    WindowPerformance* performance =
        DOMWindowPerformance::performance(*frame.DomWindow());
    if (performance &&
        (performance->HasObserverFor(PerformanceEntry::kLayoutJank) ||
         performance->ShouldBufferEntries())) {
      performance->AddLayoutJankFraction(
          RuntimeEnabledFeatures::LayoutInstabilityMoveDistanceEnabled()
              ? jank_fraction_with_move_distance
              : jank_fraction);
    }
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
}

void JankTracker::NotifyInput(const WebInputEvent& event) {
  bool event_is_meaningful =
      event.GetType() == WebInputEvent::kMouseDown ||
      event.GetType() == WebInputEvent::kKeyDown ||
      event.GetType() == WebInputEvent::kRawKeyDown ||
      // We need to explicitly include tap, as if there are no listeners, we
      // won't receive the pointer events.
      event.GetType() == WebInputEvent::kGestureTap ||
      // Ignore kPointerDown, since it might be a scroll.
      event.GetType() == WebInputEvent::kPointerUp;

  if (!event_is_meaningful)
    return;

  observed_input_or_scroll_ = true;

  // This cancels any previously scheduled task from the same timer.
  timer_.StartOneShot(kTimerDelay, FROM_HERE);
}

void JankTracker::NotifyScroll(ScrollType scroll_type) {
  // Only include user-initiated scrolls. Ignore scrolls due to e.g. hash
  // fragment navigations.
  if (scroll_type != kUserScroll && scroll_type != kCompositorScroll)
    return;

  observed_input_or_scroll_ = true;
}

bool JankTracker::IsActive() {
  // This eliminates noise from the private Page object created by
  // SVGImage::DataChanged.
  if (frame_view_->GetFrame().GetChromeClient().IsSVGImageChromeClient())
    return false;

  if (timer_.IsActive())
    return false;

  return true;
}

std::unique_ptr<TracedValue> JankTracker::PerFrameTraceData(
    double jank_fraction,
    double jank_fraction_with_move_distance,
    double granularity_scale) const {
  auto value = std::make_unique<TracedValue>();
  value->SetDouble("score", jank_fraction);
  value->SetDouble("score_with_move_distance",
                   jank_fraction_with_move_distance);
  value->SetDouble("cumulative_score", score_);
  value->SetDouble("cumulative_score_with_move_distance",
                   score_with_move_distance_);
  value->SetDouble("overall_max_distance", overall_max_distance_);
  value->SetDouble("frame_max_distance", frame_max_distance_);
  if (RuntimeEnabledFeatures::JankTrackingSweepLineEnabled())
    RegionToTracedValue(region_experimental_, granularity_scale, *value);
  else
    RegionToTracedValue(region_, granularity_scale, *value);
  value->SetBoolean("is_main_frame", frame_view_->GetFrame().IsMainFrame());
  return value;
}

std::vector<gfx::Rect> JankTracker::ConvertIntRectsToGfxRects(
    const Vector<IntRect>& int_rects,
    double granularity_scale) {
  std::vector<gfx::Rect> rects;
  for (const IntRect& rect : int_rects) {
    gfx::Rect r = gfx::Rect(
        rect.X() / granularity_scale, rect.Y() / granularity_scale,
        rect.Width() / granularity_scale, rect.Height() / granularity_scale);
    rects.emplace_back(r);
  }
  return rects;
}

void JankTracker::SetLayoutShiftRects(const Vector<IntRect>& int_rects,
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
      std::vector<gfx::Rect> rects;
      if (using_sweep_line) {
        Region old_region;
        for (IntRect rect : int_rects)
          old_region.Unite(Region(rect));
        rects =
            ConvertIntRectsToGfxRects(old_region.Rects(), granularity_scale);
      } else {
        rects = ConvertIntRectsToGfxRects(int_rects, granularity_scale);
      }
      cc_layer->layer_tree_host()->hud_layer()->SetLayoutShiftRects(rects);
      cc_layer->layer_tree_host()->hud_layer()->SetNeedsPushProperties();
    }
  }
}

}  // namespace blink
