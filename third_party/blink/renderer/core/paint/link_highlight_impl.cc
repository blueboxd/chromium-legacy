/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/paint/link_highlight_impl.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "cc/layers/picture_layer.h"
#include "cc/paint/display_item_list.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_float_point.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_base.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation_curve.h"
#include "third_party/blink/renderer/platform/animation/compositor_float_animation_curve.h"
#include "third_party/blink/renderer/platform/animation/compositor_keyframe_model.h"
#include "third_party/blink/renderer/platform/animation/compositor_target_property.h"
#include "third_party/blink/renderer/platform/animation/timing_function.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/foreign_layer_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_recorder.h"
#include "third_party/blink/renderer/platform/web_test_support.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/core/SkMatrix44.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

static constexpr float kStartOpacity = 1;

namespace {

float HighlightTargetOpacity() {
  // For web tests we don't fade out.
  return WebTestSupport::IsRunningWebTest() ? kStartOpacity : 0;
}

}  // namespace

static CompositorElementId NewElementId() {
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled() ||
      RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled()) {
    return CompositorElementIdFromUniqueObjectId(
        NewUniqueObjectId(), CompositorElementIdNamespace::kPrimaryEffect);
  }
  return CompositorElementIdFromUniqueObjectId(NewUniqueObjectId());
}

LinkHighlightImpl::LinkHighlightImpl(Node* node)
    : node_(node),
      current_graphics_layer_(nullptr),
      is_scrolling_graphics_layer_(false),
      offset_from_transform_node_(FloatPoint()),
      geometry_needs_update_(false),
      is_animating_(false),
      start_time_(CurrentTimeTicks()),
      element_id_(NewElementId()) {
  DCHECK(node_);
  fragments_.emplace_back();

  // The layer's element id is required for animating layers in layer trees.
  // When using layer lists, the element id is set on the effect node.
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled() &&
      !RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled())
    fragments_[0].Layer()->SetElementId(element_id_);

  compositor_animation_ = CompositorAnimation::Create();
  DCHECK(compositor_animation_);
  compositor_animation_->SetAnimationDelegate(this);
  compositor_animation_->AttachElement(element_id_);
  geometry_needs_update_ = true;

  EffectPaintPropertyNode::State state;
  state.opacity = HighlightTargetOpacity();
  state.local_transform_space = &TransformPaintPropertyNode::Root();
  state.compositor_element_id = element_id_;
  state.direct_compositing_reasons = CompositingReason::kActiveOpacityAnimation;
  state.has_active_opacity_animation = true;
  effect_ = EffectPaintPropertyNode::Create(EffectPaintPropertyNode::Root(),
                                            std::move(state));
#if DCHECK_IS_ON()
  effect_->SetDebugName("LinkHighlightEffect");
#endif
}

LinkHighlightImpl::~LinkHighlightImpl() {
  if (compositor_animation_->IsElementAttached())
    compositor_animation_->DetachElement();
  compositor_animation_->SetAnimationDelegate(nullptr);
  compositor_animation_.reset();

  ClearGraphicsLayerLinkHighlightPointer();
  ReleaseResources();
}

void LinkHighlightImpl::ReleaseResources() {
  if (!node_)
    return;

  if (auto* layout_object = node_->GetLayoutObject())
    layout_object->SetNeedsPaintPropertyUpdate();
  else
    SetPaintArtifactCompositorNeedsUpdate();

  node_.Clear();
}

void LinkHighlightImpl::AttachLinkHighlightToCompositingLayer(
    const LayoutBoxModelObject& paint_invalidation_container) {
  DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());

  GraphicsLayer* new_graphics_layer =
      paint_invalidation_container.Layer()->GraphicsLayerBacking(
          node_->GetLayoutObject());
  is_scrolling_graphics_layer_ = false;
  // FIXME: There should always be a GraphicsLayer. See crbug.com/431961.
  if (paint_invalidation_container.Layer()->NeedsCompositedScrolling() &&
      node_->GetLayoutObject() != &paint_invalidation_container) {
    is_scrolling_graphics_layer_ = true;
  }
  if (!new_graphics_layer) {
    ClearGraphicsLayerLinkHighlightPointer();
    return;
  }

  if (current_graphics_layer_ != new_graphics_layer) {
    if (current_graphics_layer_)
      ClearGraphicsLayerLinkHighlightPointer();

    current_graphics_layer_ = new_graphics_layer;
    current_graphics_layer_->AddLinkHighlight(this);
  }
}

static void AddQuadToPath(const FloatQuad& quad, Path& path) {
  // FIXME: Make this create rounded quad-paths, just like the axis-aligned
  // case.
  path.MoveTo(quad.P1());
  path.AddLineTo(quad.P2());
  path.AddLineTo(quad.P3());
  path.AddLineTo(quad.P4());
  path.CloseSubpath();
}

void LinkHighlightImpl::ComputeQuads(const Node& node,
                                     Vector<FloatQuad>& out_quads) const {
  DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());

  if (!node.GetLayoutObject())
    return;

  LayoutObject* layout_object = node.GetLayoutObject();

  // For inline elements, absoluteQuads will return a line box based on the
  // line-height and font metrics, which is technically incorrect as replaced
  // elements like images should use their intristic height and expand the
  // linebox  as needed. To get an appropriately sized highlight we descend
  // into the children and have them add their boxes.
  if (layout_object->IsLayoutInline()) {
    for (Node* child = LayoutTreeBuilderTraversal::FirstChild(node); child;
         child = LayoutTreeBuilderTraversal::NextSibling(*child))
      ComputeQuads(*child, out_quads);
  } else {
    // FIXME: this does not need to be absolute, just in the paint invalidation
    // container's space.
    layout_object->AbsoluteQuads(out_quads, kTraverseDocumentBoundaries);
  }
}

bool LinkHighlightImpl::ComputeHighlightLayerPathAndPosition(
    const LayoutBoxModelObject& paint_invalidation_container) {
  DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());

  if (!node_ || !node_->GetLayoutObject() || !current_graphics_layer_)
    return false;

  // FIXME: This is defensive code to avoid crashes such as those described in
  // crbug.com/440887. This should be cleaned up once we fix the root cause of
  // of the paint invalidation container not being composited.
  if (!paint_invalidation_container.Layer()->GetCompositedLayerMapping() &&
      !paint_invalidation_container.Layer()->GroupedMapping())
    return false;

  // Get quads for node in absolute coordinates.
  Vector<FloatQuad> quads;
  ComputeQuads(*node_, quads);
  Path new_path;

  for (wtf_size_t quad_index = 0; quad_index < quads.size(); ++quad_index) {
    FloatQuad absolute_quad = quads[quad_index];

    // Scrolling content layers have the same offset from layout object as the
    // non-scrolling layers. Thus we need to adjust for their scroll offset.
    if (is_scrolling_graphics_layer_) {
      FloatPoint scroll_position = paint_invalidation_container.Layer()
                                       ->GetScrollableArea()
                                       ->ScrollPosition();
      absolute_quad.Move(ToScrollOffset(scroll_position));
    }

    absolute_quad.SetP1(FloatPoint(RoundedIntPoint(absolute_quad.P1())));
    absolute_quad.SetP2(FloatPoint(RoundedIntPoint(absolute_quad.P2())));
    absolute_quad.SetP3(FloatPoint(RoundedIntPoint(absolute_quad.P3())));
    absolute_quad.SetP4(FloatPoint(RoundedIntPoint(absolute_quad.P4())));
    FloatQuad transformed_quad =
        paint_invalidation_container.AbsoluteToLocalQuad(
            absolute_quad, kTraverseDocumentBoundaries);
    PhysicalOffset offset_to_backing;

    PaintLayer::MapPointInPaintInvalidationContainerToBacking(
        paint_invalidation_container, offset_to_backing);

    // Adjust for offset from LayoutObject.
    offset_to_backing -=
        PhysicalOffset(current_graphics_layer_->OffsetFromLayoutObject());

    transformed_quad.Move(FloatSize(offset_to_backing));

    // FIXME: for now, we'll only use rounded paths if we have a single node
    // quad. The reason for this is that we may sometimes get a chain of
    // adjacent boxes (e.g. for text nodes) which end up looking like sausage
    // links: these should ideally be merged into a single rect before creating
    // the path, but that's another CL.
    if (quads.size() == 1 && transformed_quad.IsRectilinear() &&
        !node_->GetDocument()
             .GetSettings()
             ->GetMockGestureTapHighlightsEnabled()) {
      FloatSize rect_rounding_radii(3, 3);
      new_path.AddRoundedRect(transformed_quad.BoundingBox(),
                              rect_rounding_radii);
    } else {
      AddQuadToPath(transformed_quad, new_path);
    }
  }

  FloatRect bounding_rect = new_path.BoundingRect();
  new_path.Translate(-ToFloatSize(bounding_rect.Location()));

  DCHECK_EQ(1u, fragments_.size());
  fragments_[0].SetColor(
      node_->GetLayoutObject()->StyleRef().TapHighlightColor());
  auto* layer = fragments_[0].Layer();

  bool path_has_changed = new_path != fragments_[0].GetPath();
  if (path_has_changed) {
    fragments_[0].SetPath(new_path);
    layer->SetBounds(
        static_cast<gfx::Size>(EnclosingIntRect(bounding_rect).Size()));
  }

  layer->SetPosition(bounding_rect.Location());

  if (RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled()) {
    offset_from_transform_node_ =
        FloatPoint(current_graphics_layer_->GetOffsetFromTransformNode());
    offset_from_transform_node_.MoveBy(bounding_rect.Location());
    SetPaintArtifactCompositorNeedsUpdate();
  }

  return path_has_changed;
}

LinkHighlightImpl::LinkHighlightFragment::LinkHighlightFragment() {
  layer_ = cc::PictureLayer::Create(this);
  layer_->SetTransformOrigin(FloatPoint3D());
  layer_->SetIsDrawable(true);
  layer_->SetOpacity(kStartOpacity);
}

LinkHighlightImpl::LinkHighlightFragment::~LinkHighlightFragment() {
  layer_->ClearClient();
}

gfx::Rect LinkHighlightImpl::LinkHighlightFragment::PaintableRegion() {
  return gfx::Rect(layer_->bounds());
}

scoped_refptr<cc::DisplayItemList>
LinkHighlightImpl::LinkHighlightFragment::PaintContentsToDisplayList(
    PaintingControlSetting painting_control) {
  auto display_list = base::MakeRefCounted<cc::DisplayItemList>();

  PaintRecorder recorder;
  gfx::Rect record_bounds = PaintableRegion();
  cc::PaintCanvas* canvas =
      recorder.beginRecording(record_bounds.width(), record_bounds.height());

  PaintFlags flags;
  flags.setStyle(PaintFlags::kFill_Style);
  flags.setAntiAlias(true);
  flags.setColor(color_.Rgb());
  canvas->drawPath(path_.GetSkPath(), flags);

  display_list->StartPaint();
  display_list->push<cc::DrawRecordOp>(recorder.finishRecordingAsPicture());
  display_list->EndPaintOfUnpaired(record_bounds);

  display_list->Finalize();
  return display_list;
}

void LinkHighlightImpl::StartHighlightAnimationIfNeeded() {
  if (is_animating_)
    return;

  is_animating_ = true;
  // FIXME: Should duration be configurable?
  constexpr auto kFadeDuration = TimeDelta::FromMilliseconds(100);
  constexpr auto kMinPreFadeDuration = TimeDelta::FromMilliseconds(100);

  auto curve = std::make_unique<CompositorFloatAnimationCurve>();

  const auto& timing_function = *CubicBezierTimingFunction::Preset(
      CubicBezierTimingFunction::EaseType::EASE);

  curve->AddKeyframe(
      CompositorFloatKeyframe(0, kStartOpacity, timing_function));
  // Make sure we have displayed for at least minPreFadeDuration before starting
  // to fade out.
  TimeDelta extra_duration_required = std::max(
      TimeDelta(), kMinPreFadeDuration - (CurrentTimeTicks() - start_time_));
  if (!extra_duration_required.is_zero()) {
    curve->AddKeyframe(CompositorFloatKeyframe(
        extra_duration_required.InSecondsF(), kStartOpacity, timing_function));
  }
  curve->AddKeyframe(CompositorFloatKeyframe(
      (kFadeDuration + extra_duration_required).InSecondsF(),
      HighlightTargetOpacity(), timing_function));

  auto keyframe_model = std::make_unique<CompositorKeyframeModel>(
      *curve, compositor_target_property::OPACITY, 0, 0);

  compositor_animation_->AddKeyframeModel(std::move(keyframe_model));

  Invalidate();
}

void LinkHighlightImpl::ClearGraphicsLayerLinkHighlightPointer() {
  if (current_graphics_layer_) {
    current_graphics_layer_->RemoveLinkHighlight(this);
    current_graphics_layer_ = nullptr;
  }
}

void LinkHighlightImpl::NotifyAnimationStarted(double, int) {}

void LinkHighlightImpl::NotifyAnimationFinished(double, int) {
  // Since WebViewImpl may hang on to us for a while, make sure we
  // release resources as soon as possible.
  ClearGraphicsLayerLinkHighlightPointer();
  ReleaseResources();
}

void LinkHighlightImpl::UpdateGeometry() {
  DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());

  if (!node_ || !node_->GetLayoutObject()) {
    ClearGraphicsLayerLinkHighlightPointer();
    ReleaseResources();
    return;
  }

  // To avoid unnecessary updates (e.g. other entities have requested animations
  // from our WebViewImpl), only proceed if we actually requested an update.
  if (!geometry_needs_update_)
    return;

  geometry_needs_update_ = false;

  const LayoutBoxModelObject& paint_invalidation_container =
      node_->GetLayoutObject()->ContainerForPaintInvalidation();
  AttachLinkHighlightToCompositingLayer(paint_invalidation_container);
  if (ComputeHighlightLayerPathAndPosition(paint_invalidation_container)) {
    // We only need to invalidate the layer if the highlight size has changed,
    // otherwise we can just re-position the layer without needing to
    // repaint.
    Layer()->SetNeedsDisplay();

    if (current_graphics_layer_) {
      IntRect rect = IntRect(IntPoint(), IntSize(Layer()->bounds()));
      current_graphics_layer_->TrackRasterInvalidation(
          *this, rect, PaintInvalidationReason::kFullLayer);
    }
  }
}

void LinkHighlightImpl::ClearCurrentGraphicsLayer() {
  current_graphics_layer_ = nullptr;
  geometry_needs_update_ = true;
}

void LinkHighlightImpl::Invalidate() {
  // Make sure we update geometry on the next callback from
  // WebViewImpl::layout().
  geometry_needs_update_ = true;
}

cc::Layer* LinkHighlightImpl::Layer() {
  DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
  DCHECK_EQ(1u, fragments_.size());
  return fragments_[0].Layer();
}

CompositorAnimation* LinkHighlightImpl::GetCompositorAnimation() const {
  return compositor_animation_.get();
}

const EffectPaintPropertyNode& LinkHighlightImpl::Effect() const {
  return *effect_;
}

void LinkHighlightImpl::Paint(GraphicsContext& context) {
  DCHECK(RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
  if (!node_ || !node_->GetLayoutObject()) {
    ReleaseResources();
    return;
  }

  const auto* object = node_->GetLayoutObject();
  static const FloatSize rect_rounding_radii(3, 3);
  auto color = node_->GetLayoutObject()->StyleRef().TapHighlightColor();

  // For now, we'll only use rounded rects if we have a single rect because
  // otherwise we may sometimes get a chain of adjacent boxes (e.g. for text
  // nodes) which end up looking like sausage links: these should ideally be
  // merged into a single rect before creating the path.
  bool use_rounded_rects = !node_->GetDocument()
                                .GetSettings()
                                ->GetMockGestureTapHighlightsEnabled() &&
                           !object->FirstFragment().NextFragment();

  size_t index = 0;
  for (const auto* fragment = &object->FirstFragment(); fragment;
       fragment = fragment->NextFragment(), ++index) {
    auto rects = object->OutlineRects(
        fragment->PaintOffset(), NGOutlineType::kIncludeBlockVisualOverflow);
    if (rects.size() > 1)
      use_rounded_rects = false;

    Path new_path;
    for (auto& rect : rects) {
      FloatRect snapped_rect(PixelSnappedIntRect(rect));
      if (use_rounded_rects)
        new_path.AddRoundedRect(snapped_rect, rect_rounding_radii);
      else
        new_path.AddRect(snapped_rect);
    }

    if (index == fragments_.size()) {
      fragments_.emplace_back();
      // PaintArtifactCompositor needs update for the new cc::PictureLayer we
      // just created for the fragment.
      SetPaintArtifactCompositorNeedsUpdate();
    }

    auto& link_highlight_fragment = fragments_[index];
    link_highlight_fragment.SetColor(color);

    auto bounding_rect = new_path.BoundingRect();
    new_path.Translate(-ToFloatSize(bounding_rect.Location()));

    auto* layer = link_highlight_fragment.Layer();
    if (link_highlight_fragment.GetPath() != new_path) {
      link_highlight_fragment.SetPath(new_path);
      layer->SetBounds(gfx::Size(EnclosingIntRect(bounding_rect).Size()));
      layer->SetNeedsDisplay();
    }

    auto property_tree_state = fragment->LocalBorderBoxProperties();
    property_tree_state.SetEffect(Effect());
    RecordForeignLayer(context, DisplayItem::kForeignLayerLinkHighlight, layer,
                       bounding_rect.Location(), property_tree_state);
  }

  if (index < fragments_.size()) {
    fragments_.Shrink(index);
    // PaintArtifactCompositor needs update for the cc::PictureLayers we just
    // removed for the extra fragments.
    SetPaintArtifactCompositorNeedsUpdate();
  }
}

void LinkHighlightImpl::SetPaintArtifactCompositorNeedsUpdate() {
  DCHECK(node_);
  if (auto* frame_view = node_->GetDocument().View()) {
    if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
      frame_view->SetPaintArtifactCompositorNeedsUpdate();
    else
      frame_view->GraphicsLayersDidChange();
  }
}

}  // namespace blink
