/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/page/scrolling/scrolling_coordinator.h"

#include <memory>
#include <utility>

#include "build/build_config.h"
#include "cc/animation/animation_host.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/layers/layer_position_constraint.h"
#include "cc/layers/painted_overlay_scrollbar_layer.h"
#include "cc/layers/painted_scrollbar_layer.h"
#include "cc/layers/picture_layer.h"
#include "cc/layers/scrollbar_layer_interface.h"
#include "cc/layers/solid_color_scrollbar_layer.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_layer_tree_view.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_ukm_aggregator.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/page_scale_constraints_set.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/layout/geometry/transform_state.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/scrolling_coordinator_context.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scroll_animator_base.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_layer_delegate.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation_timeline.h"
#include "third_party/blink/renderer/platform/geometry/region.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace {

cc::Layer* GraphicsLayerToCcLayer(blink::GraphicsLayer* layer) {
  return layer ? layer->CcLayer() : nullptr;
}

}  // namespace

namespace blink {

ScrollingCoordinator::ScrollingCoordinator(Page* page) : page_(page) {}

ScrollingCoordinator::~ScrollingCoordinator() {
  DCHECK(!page_);
}

void ScrollingCoordinator::Trace(blink::Visitor* visitor) {
  visitor->Trace(page_);
  visitor->Trace(horizontal_scrollbars_);
  visitor->Trace(vertical_scrollbars_);
}

void ScrollingCoordinator::SetShouldHandleScrollGestureOnMainThreadRegion(
    const Region& region,
    GraphicsLayer* layer) {
  if (cc::Layer* cc_layer = GraphicsLayerToCcLayer(layer))
    cc_layer->SetNonFastScrollableRegion(RegionToCCRegion(region));
}

void ScrollingCoordinator::NotifyGeometryChanged(LocalFrameView* frame_view) {
  frame_view->GetScrollingContext()->SetScrollGestureRegionIsDirty(true);
  frame_view->GetScrollingContext()->SetTouchEventTargetRectsAreDirty(true);
  frame_view->GetScrollingContext()->SetShouldScrollOnMainThreadIsDirty(true);
}

void ScrollingCoordinator::NotifyTransformChanged(LocalFrame* frame) {
  DCHECK(frame);
  if (!frame->View())
    return;

  frame->View()->GetScrollingContext()->SetTouchEventTargetRectsAreDirty(true);
}

ScrollableArea*
ScrollingCoordinator::ScrollableAreaWithElementIdInAllLocalFrames(
    const CompositorElementId& id) {
  for (auto* frame = page_->MainFrame(); frame;
       frame = frame->Tree().TraverseNext()) {
    auto* local_frame = DynamicTo<LocalFrame>(frame);
    if (!local_frame)
      continue;

    // Find the associated scrollable area using the element id.
    if (LocalFrameView* view = local_frame->View()) {
      if (auto* scrollable = view->ScrollableAreaWithElementId(id)) {
        return scrollable;
      }
    }
  }
  // The ScrollableArea with matching ElementId does not exist in local frames.
  return nullptr;
}

void ScrollingCoordinator::DidScroll(const gfx::ScrollOffset& offset,
                                     const CompositorElementId& element_id) {
  // Find the associated scrollable area using the element id and notify it of
  // the compositor-side scroll. We explicitly do not check the VisualViewport
  // which handles scroll offset differently (see: VisualViewport::didScroll).
  // Remote frames will receive DidScroll callbacks from their own compositor.
  // The ScrollableArea with matching ElementId may have been deleted and we can
  // safely ignore the DidScroll callback.
  if (auto* scrollable =
          ScrollableAreaWithElementIdInAllLocalFrames(element_id)) {
    scrollable->DidScroll(FloatPoint(offset.x(), offset.y()));
  }
}

void ScrollingCoordinator::UpdateAfterPaint(LocalFrameView* frame_view) {
  LocalFrame* frame = &frame_view->GetFrame();
  DCHECK(frame->IsLocalRoot());

  bool scroll_gesture_region_dirty =
      frame_view->GetScrollingContext()->ScrollGestureRegionIsDirty();
  bool touch_event_rects_dirty =
      frame_view->GetScrollingContext()->TouchEventTargetRectsAreDirty();
  bool should_scroll_on_main_thread_dirty =
      frame_view->GetScrollingContext()->ShouldScrollOnMainThreadIsDirty();
  bool frame_scroller_dirty = FrameScrollerIsDirty(frame_view);

  if (!(scroll_gesture_region_dirty || touch_event_rects_dirty ||
        should_scroll_on_main_thread_dirty || frame_scroller_dirty)) {
    return;
  }

  SCOPED_UMA_AND_UKM_TIMER(frame_view->EnsureUkmAggregator(),
                           LocalFrameUkmAggregator::kScrollingCoordinator);
  TRACE_EVENT0("input", "ScrollingCoordinator::UpdateAfterPaint");

  // TODO(pdr): Move the scroll gesture region logic to use touch action rects.
  // These features are similar and do not need independent implementations.
  if (scroll_gesture_region_dirty) {
    // Compute the regions of the page where we can't handle scroll gestures on
    // the impl thread. This currently includes:
    // 1. All scrollable areas, such as subframes, overflow divs and list boxes,
    //    whose composited scrolling are not enabled. We need to do this even if
    //    the frame view whose layout was updated is not the main frame.
    // 2. Resize control areas, e.g. the small rect at the right bottom of
    //    div/textarea/iframe when CSS property "resize" is enabled.
    // 3. Plugin areas.
    Region main_thread_scrolling_region;
    Region main_thread_fixed_region;
    ComputeShouldHandleScrollGestureOnMainThreadRegion(
        frame, &main_thread_scrolling_region, &main_thread_fixed_region);

    SetShouldHandleScrollGestureOnMainThreadRegion(
        main_thread_scrolling_region,
        frame_view->GetScrollableArea()->LayerForScrolling());

    // Fixed regions will be stored on the visual viewport's scroll layer. This
    // is because a region for an area that's fixed to the layout viewport
    // won't move when the layout viewport scrolls.
    SetShouldHandleScrollGestureOnMainThreadRegion(
        main_thread_fixed_region,
        page_->GetVisualViewport().LayerForScrolling());

    frame_view->GetScrollingContext()->SetScrollGestureRegionIsDirty(false);
  }

  if (!(touch_event_rects_dirty || should_scroll_on_main_thread_dirty ||
        frame_scroller_dirty)) {
    return;
  }

  if (touch_event_rects_dirty) {
    UpdateTouchEventTargetRectsIfNeeded(frame);
    frame_view->GetScrollingContext()->SetTouchEventTargetRectsAreDirty(false);
  }

  if (should_scroll_on_main_thread_dirty ||
      frame_view->FrameIsScrollableDidChange()) {
    // When blink generates property trees, main thread scrolling reasons are
    // stored on scroll nodes.
    if (!RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled()) {
      // TODO(pdr): This also takes over scroll animations if main thread
      // reasons are present. This needs to be implemented for
      // BlinkGenPropertyTrees.
      // TODO(bokan): Does this work for scrollers other than FrameViews? If
      // not, this will need to account for root scrollers.
      SetShouldUpdateScrollLayerPositionOnMainThread(
          frame, frame_view->GetMainThreadScrollingReasons());

      // Need to update scroll on main thread reasons for subframe because
      // subframe (e.g. iframe with background-attachment:fixed) should
      // scroll on main thread while the main frame scrolls on impl.
      frame_view->UpdateSubFrameScrollOnMainReason(*frame, 0);
    }
    frame_view->GetScrollingContext()->SetShouldScrollOnMainThreadIsDirty(
        false);
  }
  frame_view->ClearFrameIsScrollableDidChange();

  // When blink generates property trees, the user input scrollable bits are
  // stored on scroll nodes instead of layers.
  if (!RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled())
    UpdateUserInputScrollable(&page_->GetVisualViewport());
}

template <typename Function>
static void ForAllPaintingGraphicsLayers(GraphicsLayer& layer,
                                         const Function& function) {
  // Don't recurse into display-locked elements.
  if (layer.Client().PaintBlockedByDisplayLockIncludingAncestors(
          DisplayLockContextLifecycleTarget::kSelf)) {
    return;
  }

  if (layer.PaintsContentOrHitTest())
    function(layer);

  for (auto* child : layer.Children())
    ForAllPaintingGraphicsLayers(*child, function);
}

// Set the touch action rects on the cc layer from the touch action data stored
// on the GraphicsLayer's paint chunks.
static void UpdateLayerTouchActionRects(GraphicsLayer& layer) {
  if (layer.Client().ShouldThrottleRendering()) {
    layer.CcLayer()->SetTouchActionRegion(cc::TouchActionRegion());
    return;
  }

  auto offset = layer.GetOffsetFromTransformNode();
  gfx::Vector2dF layer_offset = gfx::Vector2dF(offset.X(), offset.Y());
  PaintChunkSubset paint_chunks =
      PaintChunkSubset(layer.GetPaintController().PaintChunks());
  PaintArtifactCompositor::UpdateTouchActionRects(layer.CcLayer(), layer_offset,
                                                  layer.GetPropertyTreeState(),
                                                  paint_chunks);
}

static void ClearPositionConstraintExceptForLayer(GraphicsLayer* layer,
                                                  GraphicsLayer* except) {
  // When blink generates property trees, the layer position constraints are
  // not set on cc::Layer because they are only used by the cc property tree
  // builder.
  DCHECK(!RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled() &&
         !RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
  if (layer && layer != except && GraphicsLayerToCcLayer(layer)) {
    GraphicsLayerToCcLayer(layer)->SetPositionConstraint(
        cc::LayerPositionConstraint());
  }
}

static cc::LayerPositionConstraint ComputePositionConstraint(
    const PaintLayer* layer) {
  DCHECK(!RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled() &&
         !RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
  DCHECK(layer->HasCompositedLayerMapping());
  do {
    if (layer->GetLayoutObject().Style()->GetPosition() == EPosition::kFixed) {
      const LayoutObject& fixed_position_object = layer->GetLayoutObject();
      bool fixed_to_right = !fixed_position_object.Style()->Right().IsAuto();
      bool fixed_to_bottom = fixed_position_object.Style()->IsFixedToBottom();
      cc::LayerPositionConstraint constraint;
      constraint.set_is_fixed_position(true);
      constraint.set_is_fixed_to_right_edge(fixed_to_right);
      constraint.set_is_fixed_to_bottom_edge(fixed_to_bottom);
      return constraint;
    }

    layer = layer->Parent();

    // Composited layers that inherit a fixed position state will be positioned
    // with respect to the nearest compositedLayerMapping's GraphicsLayer.
    // So, once we find a layer that has its own compositedLayerMapping, we can
    // stop searching for a fixed position LayoutObject.
  } while (layer && !layer->HasCompositedLayerMapping());
  return cc::LayerPositionConstraint();
}

void ScrollingCoordinator::UpdateLayerPositionConstraint(PaintLayer* layer) {
  // When blink generates property trees, the layer position constraints are
  // not set on cc::Layer because they are only used by the cc property tree
  // builder.
  DCHECK(!RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled() &&
         !RuntimeEnabledFeatures::CompositeAfterPaintEnabled());

  DCHECK(layer->HasCompositedLayerMapping());
  CompositedLayerMapping* composited_layer_mapping =
      layer->GetCompositedLayerMapping();
  GraphicsLayer* main_layer = composited_layer_mapping->ChildForSuperlayers();

  // Avoid unnecessary commits
  ClearPositionConstraintExceptForLayer(
      composited_layer_mapping->SquashingContainmentLayer(), main_layer);
  ClearPositionConstraintExceptForLayer(
      composited_layer_mapping->AncestorClippingLayer(), main_layer);
  ClearPositionConstraintExceptForLayer(
      composited_layer_mapping->MainGraphicsLayer(), main_layer);

  if (cc::Layer* scrollable_layer = GraphicsLayerToCcLayer(main_layer))
    scrollable_layer->SetPositionConstraint(ComputePositionConstraint(layer));
}

void ScrollingCoordinator::WillDestroyScrollableArea(
    ScrollableArea* scrollable_area) {
  RemoveScrollbarLayerGroup(scrollable_area, kHorizontalScrollbar);
  RemoveScrollbarLayerGroup(scrollable_area, kVerticalScrollbar);
}

void ScrollingCoordinator::RemoveScrollbarLayerGroup(
    ScrollableArea* scrollable_area,
    ScrollbarOrientation orientation) {
  ScrollbarMap& scrollbars = orientation == kHorizontalScrollbar
                                 ? horizontal_scrollbars_
                                 : vertical_scrollbars_;
  if (std::unique_ptr<ScrollbarLayerGroup> scrollbar_layer_group =
          scrollbars.Take(scrollable_area)) {
    GraphicsLayer::UnregisterContentsLayer(scrollbar_layer_group->layer.get());
  }
}

static std::unique_ptr<ScrollingCoordinator::ScrollbarLayerGroup>
CreateScrollbarLayer(Scrollbar& scrollbar, float device_scale_factor) {
  ScrollbarTheme& theme = scrollbar.GetTheme();
  auto scrollbar_delegate =
      std::make_unique<ScrollbarLayerDelegate>(scrollbar, device_scale_factor);

  auto layer_group =
      std::make_unique<ScrollingCoordinator::ScrollbarLayerGroup>();
  if (theme.UsesOverlayScrollbars() && theme.UsesNinePatchThumbResource()) {
    auto scrollbar_layer = cc::PaintedOverlayScrollbarLayer::Create(
        std::move(scrollbar_delegate), /*scroll_element_id=*/cc::ElementId());
    scrollbar_layer->SetElementId(scrollbar.GetElementId());
    layer_group->scrollbar_layer = scrollbar_layer.get();
    layer_group->layer = std::move(scrollbar_layer);
  } else {
    auto scrollbar_layer = cc::PaintedScrollbarLayer::Create(
        std::move(scrollbar_delegate), /*scroll_element_id=*/cc::ElementId());
    scrollbar_layer->SetElementId(scrollbar.GetElementId());
    layer_group->scrollbar_layer = scrollbar_layer.get();
    layer_group->layer = std::move(scrollbar_layer);
  }

  GraphicsLayer::RegisterContentsLayer(layer_group->layer.get());

  return layer_group;
}

std::unique_ptr<ScrollingCoordinator::ScrollbarLayerGroup>
ScrollingCoordinator::CreateSolidColorScrollbarLayer(
    ScrollbarOrientation orientation,
    int thumb_thickness,
    int track_start,
    bool is_left_side_vertical_scrollbar,
    cc::ElementId element_id) {
  cc::ScrollbarOrientation cc_orientation =
      orientation == kHorizontalScrollbar ? cc::HORIZONTAL : cc::VERTICAL;
  auto scrollbar_layer = cc::SolidColorScrollbarLayer::Create(
      cc_orientation, thumb_thickness, track_start,
      is_left_side_vertical_scrollbar, cc::ElementId());
  scrollbar_layer->SetElementId(element_id);

  auto layer_group = std::make_unique<ScrollbarLayerGroup>();
  layer_group->scrollbar_layer = scrollbar_layer.get();
  layer_group->layer = std::move(scrollbar_layer);
  GraphicsLayer::RegisterContentsLayer(layer_group->layer.get());

  return layer_group;
}

static void DetachScrollbarLayer(GraphicsLayer* scrollbar_graphics_layer) {
  DCHECK(scrollbar_graphics_layer);

  scrollbar_graphics_layer->SetContentsToCcLayer(nullptr, false);
  scrollbar_graphics_layer->SetDrawsContent(true);
  scrollbar_graphics_layer->SetHitTestable(true);
}

static void SetupScrollbarLayer(
    GraphicsLayer* scrollbar_graphics_layer,
    const ScrollingCoordinator::ScrollbarLayerGroup* scrollbar_layer_group,
    cc::Layer* scrolling_layer) {
  DCHECK(scrollbar_graphics_layer);

  if (!scrolling_layer) {
    DetachScrollbarLayer(scrollbar_graphics_layer);
    return;
  }

  scrollbar_layer_group->scrollbar_layer->SetScrollElementId(
      scrolling_layer->element_id());
  scrollbar_graphics_layer->SetContentsToCcLayer(
      scrollbar_layer_group->layer.get(),
      /*prevent_contents_opaque_changes=*/false);
  scrollbar_graphics_layer->SetDrawsContent(false);
  scrollbar_graphics_layer->SetHitTestable(false);
}

void ScrollingCoordinator::AddScrollbarLayerGroup(
    ScrollableArea* scrollable_area,
    ScrollbarOrientation orientation,
    std::unique_ptr<ScrollbarLayerGroup> scrollbar_layer_group) {
  ScrollbarMap& scrollbars = orientation == kHorizontalScrollbar
                                 ? horizontal_scrollbars_
                                 : vertical_scrollbars_;
  scrollbars.insert(scrollable_area, std::move(scrollbar_layer_group));
}

ScrollingCoordinator::ScrollbarLayerGroup*
ScrollingCoordinator::GetScrollbarLayerGroup(ScrollableArea* scrollable_area,
                                             ScrollbarOrientation orientation) {
  ScrollbarMap& scrollbars = orientation == kHorizontalScrollbar
                                 ? horizontal_scrollbars_
                                 : vertical_scrollbars_;
  return scrollbars.at(scrollable_area);
}

void ScrollingCoordinator::ScrollableAreaScrollbarLayerDidChange(
    ScrollableArea* scrollable_area,
    ScrollbarOrientation orientation) {
  if (!page_ || !page_->MainFrame())
    return;

  GraphicsLayer* scrollbar_graphics_layer =
      orientation == kHorizontalScrollbar
          ? scrollable_area->LayerForHorizontalScrollbar()
          : scrollable_area->LayerForVerticalScrollbar();

  if (scrollbar_graphics_layer) {
    Scrollbar& scrollbar = orientation == kHorizontalScrollbar
                               ? *scrollable_area->HorizontalScrollbar()
                               : *scrollable_area->VerticalScrollbar();
    if (scrollbar.IsCustomScrollbar()) {
      DetachScrollbarLayer(scrollbar_graphics_layer);
      scrollbar_graphics_layer->CcLayer()->SetIsScrollbar(true);
      return;
    }

    ScrollbarLayerGroup* scrollbar_layer_group =
        GetScrollbarLayerGroup(scrollable_area, orientation);
    if (!scrollbar_layer_group) {
      Settings* settings = page_->MainFrame()->GetSettings();

      std::unique_ptr<ScrollbarLayerGroup> group;
      if (settings->GetUseSolidColorScrollbars()) {
        DCHECK(RuntimeEnabledFeatures::OverlayScrollbarsEnabled());
        group = CreateSolidColorScrollbarLayer(
            orientation, scrollbar.GetTheme().ThumbThickness(scrollbar),
            scrollbar.GetTheme().TrackPosition(scrollbar),
            scrollable_area->ShouldPlaceVerticalScrollbarOnLeft(),
            scrollable_area->GetScrollbarElementId(orientation));
      } else {
        group = CreateScrollbarLayer(scrollbar,
                                     page_->DeviceScaleFactorDeprecated());
      }

      scrollbar_layer_group = group.get();
      AddScrollbarLayerGroup(scrollable_area, orientation, std::move(group));
    }

    cc::Layer* scroll_layer =
        GraphicsLayerToCcLayer(scrollable_area->LayerForScrolling());
    SetupScrollbarLayer(scrollbar_graphics_layer, scrollbar_layer_group,
                        scroll_layer);

    // Root layer non-overlay scrollbars should be marked opaque to disable
    // blending.
    bool is_opaque_scrollbar = !scrollbar.IsOverlayScrollbar();
    scrollbar_graphics_layer->SetContentsOpaque(
        IsForMainFrame(scrollable_area) && is_opaque_scrollbar);
  } else {
    RemoveScrollbarLayerGroup(scrollable_area, orientation);
  }
}

bool ScrollingCoordinator::UpdateCompositedScrollOffset(
    ScrollableArea* scrollable_area) {
  GraphicsLayer* scroll_layer = scrollable_area->LayerForScrolling();
  if (!scroll_layer)
    return false;

  cc::Layer* cc_layer =
      GraphicsLayerToCcLayer(scrollable_area->LayerForScrolling());
  if (!cc_layer)
    return false;

  cc_layer->SetScrollOffset(
      static_cast<gfx::ScrollOffset>(scrollable_area->ScrollPosition()));
  return true;
}

void ScrollingCoordinator::ScrollableAreaScrollLayerDidChange(
    ScrollableArea* scrollable_area) {
  if (!page_ || !page_->MainFrame())
    return;

  if (!RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled())
    UpdateUserInputScrollable(scrollable_area);

  cc::Layer* cc_layer =
      GraphicsLayerToCcLayer(scrollable_area->LayerForScrolling());
  cc::Layer* container_layer =
      GraphicsLayerToCcLayer(scrollable_area->LayerForContainer());
  if (cc_layer) {
    cc_layer->SetScrollable(container_layer->bounds());

    // The scroll offset is pushed to cc::Layers separately in
    // UpdateCompositedScrolloffset. This is needed for the visual viewport
    // scroll offsets which aren't updated as part of a compositing update. In
    // BlinkGenPropertyTrees, the visual viewport sets scroll offsets directly
    // into its transform paint property nodes so this becomes unneeded.
    if (!RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled()) {
      FloatPoint scroll_position(scrollable_area->ScrollPosition());
      cc_layer->SetScrollOffset(
          static_cast<gfx::ScrollOffset>(scroll_position));
    }

    // TODO(bokan): This method shouldn't be resizing the layer geometry. That
    // happens in CompositedLayerMapping::UpdateScrollingLayerGeometry.
    PhysicalOffset subpixel_accumulation =
        scrollable_area->Layer()
            ? scrollable_area->Layer()->SubpixelAccumulation()
            : PhysicalOffset();
    PhysicalSize contents_size =
        scrollable_area->GetLayoutBox()
            ? PhysicalSize(scrollable_area->GetLayoutBox()->ScrollWidth(),
                           scrollable_area->GetLayoutBox()->ScrollHeight())
            : PhysicalSize(scrollable_area->ContentsSize());
    IntSize scroll_contents_size =
        PhysicalRect(subpixel_accumulation, contents_size).PixelSnappedSize();

    if (scrollable_area != &page_->GetVisualViewport()) {
      // The scrolling contents layer must be at least as large as its clip.
      // The visual viewport is special because the size of its scrolling
      // content depends on the page scale factor. Its scrollable content is
      // the layout viewport which is sized based on the minimum allowed page
      // scale so it actually can be smaller than its clip.
      scroll_contents_size =
          scroll_contents_size.ExpandedTo(IntSize(container_layer->bounds()));

      // VisualViewport scrolling may involve pinch zoom and gets routed through
      // WebViewImpl explicitly rather than via ScrollingCoordinator::DidScroll
      // since it needs to be set in tandem with the page scale delta.
      cc_layer->set_did_scroll_callback(WTF::BindRepeating(
          &ScrollingCoordinator::DidScroll, WrapWeakPersistent(this)));
    }

    // This call has to go through the GraphicsLayer method to preserve
    // invalidation code there.
    scrollable_area->LayerForScrolling()->SetSize(
        static_cast<gfx::Size>(scroll_contents_size));
  }
  if (ScrollbarLayerGroup* scrollbar_layer_group =
          GetScrollbarLayerGroup(scrollable_area, kHorizontalScrollbar)) {
    GraphicsLayer* horizontal_scrollbar_layer =
        scrollable_area->LayerForHorizontalScrollbar();
    if (horizontal_scrollbar_layer) {
      SetupScrollbarLayer(horizontal_scrollbar_layer, scrollbar_layer_group,
                          cc_layer);
    }
  }
  if (ScrollbarLayerGroup* scrollbar_layer_group =
          GetScrollbarLayerGroup(scrollable_area, kVerticalScrollbar)) {
    GraphicsLayer* vertical_scrollbar_layer =
        scrollable_area->LayerForVerticalScrollbar();

    if (vertical_scrollbar_layer) {
      SetupScrollbarLayer(vertical_scrollbar_layer, scrollbar_layer_group,
                          cc_layer);
    }
  }

  // Update the viewport layer registration if the outer viewport may have
  // changed.
  if (IsForRootLayer(scrollable_area))
    page_->GetChromeClient().RegisterViewportLayers();

  CompositorAnimationTimeline* timeline;
  // LocalFrameView::CompositorAnimationTimeline() can indirectly return
  // m_programmaticScrollAnimatorTimeline if it does not have its own
  // timeline.
  if (scrollable_area->IsPaintLayerScrollableArea()) {
    timeline = ToPaintLayerScrollableArea(scrollable_area)
                   ->GetCompositorAnimationTimeline();
  } else {
    timeline = programmatic_scroll_animator_timeline_.get();
  }
  scrollable_area->LayerForScrollingDidChange(timeline);

  return;
}

void ScrollingCoordinator::UpdateTouchEventTargetRectsIfNeeded(
    LocalFrame* frame) {
  TRACE_EVENT0("input",
               "ScrollingCoordinator::updateTouchEventTargetRectsIfNeeded");

  // TODO(chrishtr): implement touch event target rects for CAP.
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  auto* view_layer = frame->View()->GetLayoutView()->Layer();
  if (auto* root = view_layer->Compositor()->PaintRootGraphicsLayer())
    ForAllPaintingGraphicsLayers(*root, UpdateLayerTouchActionRects);
}

void ScrollingCoordinator::UpdateUserInputScrollable(
    ScrollableArea* scrollable_area) {
  // When blink generates property trees, the user input scrollable bits are
  // stored on scroll nodes instead of layers so this is not needed.
  DCHECK(!RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled());
  cc::Layer* cc_layer =
      GraphicsLayerToCcLayer(scrollable_area->LayerForScrolling());
  if (cc_layer) {
    bool can_scroll_x =
        scrollable_area->UserInputScrollable(kHorizontalScrollbar);
    bool can_scroll_y =
        scrollable_area->UserInputScrollable(kVerticalScrollbar);
    cc_layer->SetUserScrollable(can_scroll_x, can_scroll_y);
  }
}

void ScrollingCoordinator::Reset(LocalFrame* frame) {
  for (const auto& scrollbar : horizontal_scrollbars_)
    GraphicsLayer::UnregisterContentsLayer(scrollbar.value->layer.get());
  for (const auto& scrollbar : vertical_scrollbars_)
    GraphicsLayer::UnregisterContentsLayer(scrollbar.value->layer.get());

  horizontal_scrollbars_.clear();
  vertical_scrollbars_.clear();
  frame->View()->ClearFrameIsScrollableDidChange();
}

void ScrollingCoordinator::TouchEventTargetRectsDidChange(LocalFrame* frame) {
  if (!frame)
    return;

  // If frame is not a local root, then the call to StaleInCompositingMode()
  // below may unexpectedly fail.
  DCHECK(frame->IsLocalRoot());
  LocalFrameView* frame_view = frame->View();
  if (!frame_view)
    return;

  // Wait until after layout to update.
  // TODO(pdr): This check is wrong as we need to mark the rects as dirty
  // regardless of whether the frame view needs layout. Remove this check.
  if (frame_view->NeedsLayout())
    return;

  // FIXME: scheduleAnimation() is just a method of forcing the compositor to
  // realize that it needs to commit here. We should expose a cleaner API for
  // this.
  auto* layout_view = frame->ContentLayoutObject();
  if (layout_view && layout_view->Compositor() &&
      layout_view->Compositor()->StaleInCompositingMode()) {
    frame_view->ScheduleAnimation();
  }

  frame_view->GetScrollingContext()->SetTouchEventTargetRectsAreDirty(true);
}

void ScrollingCoordinator::UpdateScrollParentForGraphicsLayer(
    GraphicsLayer* child,
    const PaintLayer* parent) {
  cc::Layer* scroll_parent_cc_layer = nullptr;
  if (parent && parent->HasCompositedLayerMapping())
    scroll_parent_cc_layer = GraphicsLayerToCcLayer(
        parent->GetCompositedLayerMapping()->ScrollingContentsLayer());

  child->SetScrollParent(scroll_parent_cc_layer);
}

void ScrollingCoordinator::UpdateClipParentForGraphicsLayer(
    GraphicsLayer* child,
    const PaintLayer* parent) {
  cc::Layer* clip_parent_cc_layer = nullptr;
  if (parent && parent->HasCompositedLayerMapping()) {
    clip_parent_cc_layer = GraphicsLayerToCcLayer(
        parent->GetCompositedLayerMapping()->ParentForSublayers());
  }

  child->SetClipParent(clip_parent_cc_layer);
}

void ScrollingCoordinator::SetShouldUpdateScrollLayerPositionOnMainThread(
    LocalFrame* frame,
    MainThreadScrollingReasons main_thread_scrolling_reasons) {
  VisualViewport& visual_viewport = frame->GetPage()->GetVisualViewport();
  GraphicsLayer* visual_viewport_layer = visual_viewport.ScrollLayer();
  cc::Layer* visual_viewport_scroll_layer =
      GraphicsLayerToCcLayer(visual_viewport_layer);
  // TODO(bokan): It would probably make more sense to use the root scroller's
  // layer here, but this code is only ever executed in !BGPT mode. With BGPT
  // the MainThreadScrollingReasons are already stored on individual
  // ScrollNodes. The CompositorAnimation hand-off should probably be
  // generalized to work on non-FrameView scrollers though.
  ScrollableArea* scrollable_area = frame->View()->LayoutViewport();
  GraphicsLayer* layer = scrollable_area->LayerForScrolling();
  if (cc::Layer* scroll_layer = GraphicsLayerToCcLayer(layer)) {
    if (main_thread_scrolling_reasons) {
      if (ScrollAnimatorBase* scroll_animator =
              scrollable_area->ExistingScrollAnimator()) {
        DCHECK(RuntimeEnabledFeatures::CompositeAfterPaintEnabled() ||
               frame->GetDocument()->Lifecycle().GetState() >=
                   DocumentLifecycle::kCompositingClean);
        scroll_animator->TakeOverCompositorAnimation();
      }
      scroll_layer->AddMainThreadScrollingReasons(
          main_thread_scrolling_reasons);
      if (visual_viewport_scroll_layer) {
        if (ScrollAnimatorBase* scroll_animator =
                visual_viewport.ExistingScrollAnimator()) {
          DCHECK(RuntimeEnabledFeatures::CompositeAfterPaintEnabled() ||
                 frame->GetDocument()->Lifecycle().GetState() >=
                     DocumentLifecycle::kCompositingClean);
          scroll_animator->TakeOverCompositorAnimation();
        }
        visual_viewport_scroll_layer->AddMainThreadScrollingReasons(
            main_thread_scrolling_reasons);
      }
    } else {
      // Clear all main thread scrolling reasons except the one that's set
      // if there is a running scroll animation.
      uint32_t main_thread_scrolling_reasons_to_clear = ~0u;
      main_thread_scrolling_reasons_to_clear &=
          ~cc::MainThreadScrollingReason::kHandlingScrollFromMainThread;
      scroll_layer->ClearMainThreadScrollingReasons(
          main_thread_scrolling_reasons_to_clear);
      if (visual_viewport_scroll_layer)
        visual_viewport_scroll_layer->ClearMainThreadScrollingReasons(
            main_thread_scrolling_reasons_to_clear);
    }
  }
}

void ScrollingCoordinator::LayerTreeViewInitialized(
    WebLayerTreeView& layer_tree_view,
    cc::AnimationHost& animation_host,
    LocalFrameView* view) {
  if (!Platform::Current()->IsThreadedAnimationEnabled())
    return;

  auto timeline = std::make_unique<CompositorAnimationTimeline>();
  if (view && view->GetFrame().LocalFrameRoot() != page_->MainFrame()) {
    view->GetScrollingContext()->SetAnimationHost(&animation_host);
    view->GetScrollingContext()->SetAnimationTimeline(std::move(timeline));
    view->GetCompositorAnimationHost()->AddAnimationTimeline(
        view->GetCompositorAnimationTimeline()->GetAnimationTimeline());
  } else {
    animation_host_ = &animation_host;
    programmatic_scroll_animator_timeline_ = std::move(timeline);
    animation_host_->AddAnimationTimeline(
        programmatic_scroll_animator_timeline_->GetAnimationTimeline());
  }
}

void ScrollingCoordinator::WillCloseLayerTreeView(
    WebLayerTreeView& layer_tree_view,
    LocalFrameView* view) {
  if (view && view->GetFrame().LocalFrameRoot() != page_->MainFrame()) {
    view->GetCompositorAnimationHost()->RemoveAnimationTimeline(
        view->GetCompositorAnimationTimeline()->GetAnimationTimeline());
    view->GetScrollingContext()->SetAnimationTimeline(nullptr);
    view->GetScrollingContext()->SetAnimationHost(nullptr);
  } else if (programmatic_scroll_animator_timeline_) {
    animation_host_->RemoveAnimationTimeline(
        programmatic_scroll_animator_timeline_->GetAnimationTimeline());
    programmatic_scroll_animator_timeline_ = nullptr;
    animation_host_ = nullptr;
  }
}

void ScrollingCoordinator::WillBeDestroyed() {
  DCHECK(page_);

  page_ = nullptr;
  for (const auto& scrollbar : horizontal_scrollbars_)
    GraphicsLayer::UnregisterContentsLayer(scrollbar.value->layer.get());
  for (const auto& scrollbar : vertical_scrollbars_)
    GraphicsLayer::UnregisterContentsLayer(scrollbar.value->layer.get());
}

bool ScrollingCoordinator::CoordinatesScrollingForFrameView(
    LocalFrameView* frame_view) const {
  DCHECK(IsMainThread());

  // We currently only support composited mode.
  auto* layout_view = frame_view->GetFrame().ContentLayoutObject();
  if (!layout_view)
    return false;
  return layout_view->UsesCompositing();
}

namespace {

bool ScrollsWithRootFrame(LayoutObject* object) {
  DCHECK(object);
  DCHECK(object->GetFrame());

  // TODO(bokan): Speculative fix for https://crbug.com/964293.
  if (!object || !object->GetNode())
    return true;

  const LocalFrame& frame = *object->GetFrame();

  // If we're in an iframe document, we need to determine if the containing
  // <iframe> element scrolls with the root frame.
  if (&frame != &frame.LocalFrameRoot()) {
    DCHECK(frame.GetDocument());
    DCHECK(frame.GetDocument()->LocalOwner());
    DCHECK(frame.GetDocument()->LocalOwner()->GetLayoutObject());
    return ScrollsWithRootFrame(
        frame.GetDocument()->LocalOwner()->GetLayoutObject());
  }

  DCHECK(object->EnclosingLayer());
  if (object->EnclosingLayer()->AncestorScrollingLayer() ==
      frame.ContentLayoutObject()->Layer())
    return true;

  // TODO(bokan): Returning false unconditionally here is actually wrong but we
  // do this because NonFastScrollableRegions are stored only on the layout and
  // visual viewport's scroll layers. This can lead to issues when subscroller
  // layers are scrolled (e.g. https://crbug.com/817600). These should really
  // be stored on the nearest ancestor ScrollNode, rather than the root
  // viewport.  Should be addressed by https://crbug.com/864567.
  return false;
}

}  // namespace

void ScrollingCoordinator::ComputeShouldHandleScrollGestureOnMainThreadRegion(
    const LocalFrame* frame,
    Region* scrolling_region,
    Region* fixed_region) const {
  LocalFrameView* frame_view = frame->View();
  DCHECK(scrolling_region);
  DCHECK(fixed_region);

  if (!frame_view || frame_view->ShouldThrottleRendering() ||
      !frame_view->IsVisible()) {
    return;
  }

  LocalFrameView* local_root_view = frame->LocalFrameRoot().View();

  if (const LocalFrameView::ScrollableAreaSet* scrollable_areas =
          frame_view->ScrollableAreas()) {
    for (const ScrollableArea* scrollable_area : *scrollable_areas) {
      // Composited scrollable areas can be scrolled off the main thread.
      if (scrollable_area->UsesCompositedScrolling())
        continue;

      if (ScrollsWithRootFrame(scrollable_area->GetLayoutBox())) {
        scrolling_region->Unite(scrollable_area->ScrollableAreaBoundingBox());
      } else {
        fixed_region->Unite(local_root_view->DocumentToFrame(
            scrollable_area->ScrollableAreaBoundingBox()));
      }
    }
  }

  // We use GestureScrollBegin/Update/End for moving the resizer handle. So we
  // mark these small resizer areas as non-fast-scrollable to allow the scroll
  // gestures to be passed to main thread if they are targeting the resizer
  // area. (Resizing is done in EventHandler.cpp on main thread).
  if (const LocalFrameView::ResizerAreaSet* resizer_areas =
          frame_view->ResizerAreas()) {
    for (const LayoutBox* box : *resizer_areas) {
      PaintLayerScrollableArea* scrollable_area =
          box->Layer()->GetScrollableArea();

      IntRect bounds_in_frame = box->AbsoluteBoundingBoxRect();
      IntRect corner_in_frame =
          scrollable_area->ResizerCornerRect(bounds_in_frame, kResizerForTouch);

      IntRect corner_in_root_frame =
          EnclosingIntRect(scrollable_area->GetLayoutBox()->LocalToAbsoluteRect(
              PhysicalRect(corner_in_frame), kTraverseDocumentBoundaries));

      if (ScrollsWithRootFrame(scrollable_area->GetLayoutBox())) {
        scrolling_region->Unite(
            local_root_view->FrameToDocument(corner_in_root_frame));
      } else {
        fixed_region->Unite(corner_in_root_frame);
      }
    }
  }

  for (const auto& plugin : frame_view->Plugins()) {
    Element* element = plugin->GetElement();
    // Plugins can run script during layout so ensure the plugin wasn't removed
    // since being added to the Plugins set.
    if (!element->GetLayoutObject())
      continue;

    if (plugin->WantsWheelEvents()) {
      IntRect box = frame_view->ConvertToRootFrame(plugin->FrameRect());
      if (ScrollsWithRootFrame(element->GetLayoutObject())) {
        scrolling_region->Unite(local_root_view->FrameToDocument(box));
      } else {
        fixed_region->Unite(box);
      }
    }
  }

  const FrameTree& tree = frame->Tree();
  for (Frame* sub_frame = tree.FirstChild(); sub_frame;
       sub_frame = sub_frame->Tree().NextSibling()) {
    if (auto* sub_local_frame = DynamicTo<LocalFrame>(sub_frame)) {
      ComputeShouldHandleScrollGestureOnMainThreadRegion(
          sub_local_frame, scrolling_region, fixed_region);
    }
  }
}

void ScrollingCoordinator::
    FrameViewHasBackgroundAttachmentFixedObjectsDidChange(
        LocalFrameView* frame_view) {
  DCHECK(IsMainThread());
  DCHECK(frame_view);

  if (!CoordinatesScrollingForFrameView(frame_view))
    return;

  frame_view->GetScrollingContext()->SetShouldScrollOnMainThreadIsDirty(true);
}

void ScrollingCoordinator::FrameViewFixedObjectsDidChange(
    LocalFrameView* frame_view) {
  DCHECK(IsMainThread());
  DCHECK(frame_view);

  if (!CoordinatesScrollingForFrameView(frame_view))
    return;

  frame_view->GetScrollingContext()->SetShouldScrollOnMainThreadIsDirty(true);
}

bool ScrollingCoordinator::IsForRootLayer(
    ScrollableArea* scrollable_area) const {
  if (!IsA<LocalFrame>(page_->MainFrame()))
    return false;

  // FIXME(305811): Refactor for OOPI.
  if (auto* layout_view =
          page_->DeprecatedLocalMainFrame()->View()->GetLayoutView())
    return scrollable_area == layout_view->Layer()->GetScrollableArea();
  return false;
}

bool ScrollingCoordinator::IsForMainFrame(
    ScrollableArea* scrollable_area) const {
  if (!IsA<LocalFrame>(page_->MainFrame()))
    return false;

  // FIXME(305811): Refactor for OOPI.
  return scrollable_area ==
         page_->DeprecatedLocalMainFrame()->View()->LayoutViewport();
}

void ScrollingCoordinator::FrameViewRootLayerDidChange(
    LocalFrameView* frame_view) {
  DCHECK(IsMainThread());
  DCHECK(page_);

  if (!CoordinatesScrollingForFrameView(frame_view))
    return;

  NotifyGeometryChanged(frame_view);
}

bool ScrollingCoordinator::FrameScrollerIsDirty(
    LocalFrameView* frame_view) const {
  DCHECK(frame_view);
  // TODO(bokan): This should probably be checking the root scroller in the
  // FrameView, rather than the frame_view.
  if (frame_view->FrameIsScrollableDidChange())
    return true;

  if (cc::Layer* scroll_layer =
          frame_view ? GraphicsLayerToCcLayer(
                           frame_view->LayoutViewport()->LayerForScrolling())
                     : nullptr) {
    return static_cast<gfx::Size>(
               frame_view->LayoutViewport()->ContentsSize()) !=
           scroll_layer->bounds();
  }
  return false;
}

}  // namespace blink
