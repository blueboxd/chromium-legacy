// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/property_tree_builder.h"

#include <stddef.h>

#include <map>
#include <set>

#include "base/auto_reset.h"
#include "cc/base/math_util.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/picture_layer.h"
#include "cc/trees/clip_node.h"
#include "cc/trees/draw_property_utils.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/mutator_host.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/transform_node.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace cc {

namespace {

struct DataForRecursion {
  int transform_tree_parent;
  int clip_tree_parent;
  int effect_tree_parent;
  int scroll_tree_parent;
  int closest_ancestor_with_cached_render_surface;
  int closest_ancestor_with_copy_request;
  uint32_t main_thread_scrolling_reasons;
  SkColor safe_opaque_background_color;
  bool in_subtree_of_page_scale_layer;
  bool affected_by_outer_viewport_bounds_delta;
  bool should_flatten;
  bool scroll_tree_parent_created_by_uninheritable_criteria;
  bool animation_axis_aligned_since_render_target;
  bool not_axis_aligned_since_last_clip;
  gfx::Transform compound_transform_since_render_target;
  bool* subtree_has_rounded_corner;
};

class PropertyTreeBuilderContext {
 public:
  PropertyTreeBuilderContext(Layer* root_layer,
                             const Layer* page_scale_layer,
                             const Layer* inner_viewport_scroll_layer,
                             const Layer* outer_viewport_scroll_layer,
                             const ElementId overscroll_elasticity_element_id,
                             const gfx::Vector2dF& elastic_overscroll,
                             float page_scale_factor,
                             const gfx::Transform& device_transform,
                             MutatorHost* mutator_host,
                             PropertyTrees* property_trees)
      : root_layer_(root_layer),
        page_scale_layer_(page_scale_layer),
        inner_viewport_scroll_layer_(inner_viewport_scroll_layer),
        outer_viewport_scroll_layer_(outer_viewport_scroll_layer),
        overscroll_elasticity_element_id_(overscroll_elasticity_element_id),
        elastic_overscroll_(elastic_overscroll),
        page_scale_factor_(page_scale_factor),
        device_transform_(device_transform),
        mutator_host_(*mutator_host),
        property_trees_(*property_trees),
        transform_tree_(property_trees->transform_tree),
        clip_tree_(property_trees->clip_tree),
        effect_tree_(property_trees->effect_tree),
        scroll_tree_(property_trees->scroll_tree) {}

  void BuildPropertyTrees(float device_scale_factor,
                          const gfx::Rect& viewport,
                          SkColor root_background_color) const;

 private:
  void BuildPropertyTreesInternal(
      Layer* layer,
      const DataForRecursion& data_from_parent) const;

  bool AddTransformNodeIfNeeded(const DataForRecursion& data_from_ancestor,
                                Layer* layer,
                                bool created_render_surface,
                                DataForRecursion* data_for_children) const;

  void AddClipNodeIfNeeded(const DataForRecursion& data_from_ancestor,
                           Layer* layer,
                           bool created_transform_node,
                           DataForRecursion* data_for_children) const;

  bool AddEffectNodeIfNeeded(const DataForRecursion& data_from_ancestor,
                             Layer* layer,
                             DataForRecursion* data_for_children) const;

  void AddScrollNodeIfNeeded(const DataForRecursion& data_from_ancestor,
                             Layer* layer,
                             DataForRecursion* data_for_children) const;

  bool UpdateRenderSurfaceIfNeeded(int parent_effect_tree_id,
                                   DataForRecursion* data_for_children,
                                   bool subtree_has_rounded_corner,
                                   bool created_transform_node) const;

  Layer* root_layer_;
  const Layer* page_scale_layer_;
  const Layer* inner_viewport_scroll_layer_;
  const Layer* outer_viewport_scroll_layer_;
  const ElementId overscroll_elasticity_element_id_;
  const gfx::Vector2dF elastic_overscroll_;
  float page_scale_factor_;
  const gfx::Transform& device_transform_;
  MutatorHost& mutator_host_;
  PropertyTrees& property_trees_;
  TransformTree& transform_tree_;
  ClipTree& clip_tree_;
  EffectTree& effect_tree_;
  ScrollTree& scroll_tree_;
};

// Methods to query state from the AnimationHost ----------------------
bool OpacityIsAnimating(const MutatorHost& host, Layer* layer) {
  return host.IsAnimatingOpacityProperty(layer->element_id(),
                                         layer->GetElementTypeForAnimation());
}

bool HasPotentiallyRunningOpacityAnimation(const MutatorHost& host,
                                           Layer* layer) {
  return host.HasPotentiallyRunningOpacityAnimation(
      layer->element_id(), layer->GetElementTypeForAnimation());
}

bool HasPotentialOpacityAnimation(const MutatorHost& host, Layer* layer) {
  return HasPotentiallyRunningOpacityAnimation(host, layer) ||
         layer->OpacityCanAnimateOnImplThread();
}

bool FilterIsAnimating(const MutatorHost& host, Layer* layer) {
  return host.IsAnimatingFilterProperty(layer->element_id(),
                                        layer->GetElementTypeForAnimation());
}

bool HasPotentiallyRunningFilterAnimation(const MutatorHost& host,
                                          Layer* layer) {
  return host.HasPotentiallyRunningFilterAnimation(
      layer->element_id(), layer->GetElementTypeForAnimation());
}

bool TransformIsAnimating(const MutatorHost& host, Layer* layer) {
  return host.IsAnimatingTransformProperty(layer->element_id(),
                                           layer->GetElementTypeForAnimation());
}

bool HasPotentiallyRunningTransformAnimation(const MutatorHost& host,
                                             Layer* layer) {
  return host.HasPotentiallyRunningTransformAnimation(
      layer->element_id(), layer->GetElementTypeForAnimation());
}

void GetAnimationScales(const MutatorHost& host,
                        Layer* layer,
                        float* maximum_scale,
                        float* starting_scale) {
  return host.GetAnimationScales(layer->element_id(),
                                 layer->GetElementTypeForAnimation(),
                                 maximum_scale, starting_scale);
}

bool AnimationsPreserveAxisAlignment(const MutatorHost& host, Layer* layer) {
  return host.AnimationsPreserveAxisAlignment(layer->element_id());
}

bool HasAnyAnimationTargetingProperty(const MutatorHost& host,
                                      Layer* layer,
                                      TargetProperty::Type property) {
  return host.HasAnyAnimationTargetingProperty(layer->element_id(), property);
}

// -------------------------------------------------------------------

bool LayerClipsSubtreeToItsBounds(Layer* layer) {
  return layer->masks_to_bounds() || layer->mask_layer();
}

bool LayerClipsSubtree(Layer* layer) {
  return LayerClipsSubtreeToItsBounds(layer) || layer->HasRoundedCorner() ||
         !layer->clip_rect().IsEmpty();
}

gfx::RRectF RoundedCornerBounds(Layer* layer) {
  return gfx::RRectF(layer->EffectiveClipRect(), layer->corner_radii());
}

void PropertyTreeBuilderContext::AddClipNodeIfNeeded(
    const DataForRecursion& data_from_ancestor,
    Layer* layer,
    bool created_transform_node,
    DataForRecursion* data_for_children) const {
  const int parent_id = data_from_ancestor.clip_tree_parent;

  bool layer_clips_subtree = LayerClipsSubtree(layer);
  bool requires_node =
      layer_clips_subtree || layer->filters().HasFilterThatMovesPixels();
  if (!requires_node) {
    data_for_children->clip_tree_parent = parent_id;
  } else {
    ClipNode node;
    node.clip = layer->EffectiveClipRect();

    // Move the clip bounds so that it is relative to the transform parent.
    node.clip += layer->offset_to_transform_parent();

    node.transform_id = created_transform_node
                            ? data_for_children->transform_tree_parent
                            : data_from_ancestor.transform_tree_parent;
    if (layer_clips_subtree) {
      node.clip_type = ClipNode::ClipType::APPLIES_LOCAL_CLIP;
    } else {
      DCHECK(layer->filters().HasFilterThatMovesPixels());
      node.clip_type = ClipNode::ClipType::EXPANDS_CLIP;
      node.clip_expander = ClipExpander(layer->effect_tree_index());
    }
    data_for_children->clip_tree_parent = clip_tree_.Insert(node, parent_id);
  }

  layer->SetHasClipNode(requires_node);
  layer->SetClipTreeIndex(data_for_children->clip_tree_parent);
}

bool IsAtBoundaryOf3dRenderingContext(Layer* layer) {
  if (layer->parent())
    return layer->parent()->sorting_context_id() != layer->sorting_context_id();
  return layer->sorting_context_id() != 0;
}

bool PropertyTreeBuilderContext::AddTransformNodeIfNeeded(
    const DataForRecursion& data_from_ancestor,
    Layer* layer,
    bool created_render_surface,
    DataForRecursion* data_for_children) const {
  const bool is_root = !layer->parent();
  const bool is_page_scale_layer = layer == page_scale_layer_;
  const bool is_overscroll_elasticity_layer =
      overscroll_elasticity_element_id_ &&
      layer->element_id() == overscroll_elasticity_element_id_;
  const bool is_scrollable = layer->scrollable();
  // Scrolling a layer should not move it from being pixel-aligned to moving off
  // the pixel grid and becoming fuzzy. So always snap scrollable things to the
  // pixel grid. Layers may also request to be snapped as such.
  const bool is_snapped =
      is_scrollable || layer->IsSnappedToPixelGridInTarget();

  const bool has_significant_transform =
      !layer->transform().IsIdentityOr2DTranslation();

  const bool has_potentially_animated_transform =
      HasPotentiallyRunningTransformAnimation(mutator_host_, layer);

  // A transform node is needed even for a finished animation, since differences
  // in the timing of animation state updates can mean that an animation that's
  // in the Finished state at tree-building time on the main thread is still in
  // the Running state right after commit on the compositor thread.
  const bool has_any_transform_animation = HasAnyAnimationTargetingProperty(
      mutator_host_, layer, TargetProperty::TRANSFORM);

  const bool has_surface = created_render_surface;

  const bool is_at_boundary_of_3d_rendering_context =
      IsAtBoundaryOf3dRenderingContext(layer);

  DCHECK(!is_scrollable || is_snapped);
  bool requires_node = is_root || is_snapped || has_significant_transform ||
                       has_any_transform_animation || has_surface ||
                       is_page_scale_layer || is_overscroll_elasticity_layer ||
                       is_at_boundary_of_3d_rendering_context ||
                       layer->HasRoundedCorner();

  int parent_index = TransformTree::kRootNodeId;
  gfx::Vector2dF parent_offset;
  if (!is_root) {
    parent_index = data_from_ancestor.transform_tree_parent;
    // Now layer tree mode (IsUsingLayerLists is false) is for ui compositor
    // only. The transform tree hierarchy is always the same as layer hierarchy.
    DCHECK_EQ(parent_index, layer->parent()->transform_tree_index());
    parent_offset = layer->parent()->offset_to_transform_parent();
  }

  if (!requires_node) {
    data_for_children->should_flatten |= layer->should_flatten_transform();
    gfx::Vector2dF local_offset = layer->position().OffsetFromOrigin() +
                                  layer->transform().To2dTranslation();
    layer->SetOffsetToTransformParent(parent_offset + local_offset);
    layer->SetShouldFlattenScreenSpaceTransformFromPropertyTree(
        data_from_ancestor.should_flatten);
    layer->SetTransformTreeIndex(parent_index);
    return false;
  }

  transform_tree_.Insert(TransformNode(), parent_index);
  TransformNode* node = transform_tree_.back();
  layer->SetTransformTreeIndex(node->id);
  data_for_children->transform_tree_parent = node->id;

  // For animation subsystem purposes, if this layer has a compositor element
  // id, we build a map from that id to this transform node.
  if (layer->element_id()) {
    property_trees_.element_id_to_transform_node_index[layer->element_id()] =
        node->id;
    node->element_id = layer->element_id();
  }

  node->scrolls = is_scrollable;
  node->should_be_snapped = is_snapped;
  node->flattens_inherited_transform = data_for_children->should_flatten;
  node->sorting_context_id = layer->sorting_context_id();

  if (is_root || is_page_scale_layer) {
    // Root layer and page scale layer should not have transform or offset.
    DCHECK(layer->position().IsOrigin());
    DCHECK(parent_offset.IsZero());
    DCHECK(layer->transform().IsIdentity());

    if (is_root) {
      DCHECK(!is_page_scale_layer);
      transform_tree_.SetRootScaleAndTransform(
          transform_tree_.device_scale_factor(), device_transform_);
    } else {
      DCHECK(is_page_scale_layer);
      transform_tree_.set_page_scale_factor(page_scale_factor_);
      node->local.Scale(page_scale_factor_, page_scale_factor_);
      data_for_children->in_subtree_of_page_scale_layer = true;
    }
  } else {
    node->local = layer->transform();
    node->origin = layer->transform_origin();
    node->post_translation =
        parent_offset + layer->position().OffsetFromOrigin();
  }

  node->in_subtree_of_page_scale_layer =
      data_for_children->in_subtree_of_page_scale_layer;

  // Surfaces inherently flatten transforms.
  data_for_children->should_flatten =
      layer->should_flatten_transform() || has_surface;

  node->has_potential_animation = has_potentially_animated_transform;
  node->is_currently_animating = TransformIsAnimating(mutator_host_, layer);
  GetAnimationScales(mutator_host_, layer, &node->maximum_animation_scale,
                     &node->starting_animation_scale);

  if (is_overscroll_elasticity_layer) {
    DCHECK(!is_scrollable);
    node->scroll_offset = gfx::ScrollOffset(elastic_overscroll_);
  } else {
    node->scroll_offset = layer->CurrentScrollOffset();
  }

  node->needs_local_transform_update = true;
  transform_tree_.UpdateTransforms(node->id);

  layer->SetOffsetToTransformParent(gfx::Vector2dF());

  // Flattening (if needed) will be handled by |node|.
  layer->SetShouldFlattenScreenSpaceTransformFromPropertyTree(false);

  return true;
}

bool LayerIsInExisting3DRenderingContext(Layer* layer) {
  return layer->sorting_context_id() != 0 && layer->parent() &&
         layer->parent()->sorting_context_id() != 0 &&
         layer->parent()->sorting_context_id() == layer->sorting_context_id();
}

RenderSurfaceReason ComputeRenderSurfaceReason(const MutatorHost& mutator_host,
                                               Layer* layer,
                                               gfx::Transform current_transform,
                                               bool animation_axis_aligned) {
  const bool preserves_2d_axis_alignment =
      current_transform.Preserves2dAxisAlignment() && animation_axis_aligned;
  const bool is_root = !layer->parent();
  if (is_root)
    return RenderSurfaceReason::kRoot;

  // If the layer uses a mask.
  if (layer->mask_layer()) {
    return RenderSurfaceReason::kMask;
  }

  // If the layer uses trilinear filtering.
  if (layer->trilinear_filtering()) {
    return RenderSurfaceReason::kTrilinearFiltering;
  }

  // If the layer uses a CSS filter.
  if (!layer->filters().IsEmpty()) {
    return RenderSurfaceReason::kFilter;
  }

  // If the layer uses a CSS backdrop-filter.
  if (!layer->backdrop_filters().IsEmpty()) {
    return RenderSurfaceReason::kBackdropFilter;
  }

  // If the layer will use a CSS filter.  In this case, the animation
  // will start and add a filter to this layer, so it needs a surface.
  if (HasPotentiallyRunningFilterAnimation(mutator_host, layer)) {
    return RenderSurfaceReason::kFilterAnimation;
  }

  int num_descendants_that_draw_content =
      layer->NumDescendantsThatDrawContent();

  // If the layer flattens its subtree, but it is treated as a 3D object by its
  // parent (i.e. parent participates in a 3D rendering context).
  if (LayerIsInExisting3DRenderingContext(layer) &&
      layer->should_flatten_transform() &&
      num_descendants_that_draw_content > 0) {
    return RenderSurfaceReason::k3dTransformFlattening;
  }

  if (!layer->is_fast_rounded_corner() && layer->HasRoundedCorner() &&
      num_descendants_that_draw_content > 1) {
    return RenderSurfaceReason::kRoundedCorner;
  }

  // If the layer has blending.
  // TODO(rosca): this is temporary, until blending is implemented for other
  // types of quads than viz::RenderPassDrawQuad. Layers having descendants that
  // draw content will still create a separate rendering surface.
  if (layer->blend_mode() != SkBlendMode::kSrcOver) {
    return RenderSurfaceReason::kBlendMode;
  }
  // If the layer clips its descendants but it is not axis-aligned with respect
  // to its parent.
  bool layer_clips_external_content = LayerClipsSubtree(layer);
  if (layer_clips_external_content && !preserves_2d_axis_alignment &&
      num_descendants_that_draw_content > 0) {
    return RenderSurfaceReason::kClipAxisAlignment;
  }

  // If the layer has some translucency and does not have a preserves-3d
  // transform style.  This condition only needs a render surface if two or more
  // layers in the subtree overlap. But checking layer overlaps is unnecessarily
  // costly so instead we conservatively create a surface whenever at least two
  // layers draw content for this subtree.
  bool at_least_two_layers_in_subtree_draw_content =
      num_descendants_that_draw_content > 0 &&
      (layer->DrawsContent() || num_descendants_that_draw_content > 1);

  bool may_have_transparency =
      layer->EffectiveOpacity() != 1.f ||
      HasPotentiallyRunningOpacityAnimation(mutator_host, layer);
  if (may_have_transparency && layer->should_flatten_transform() &&
      at_least_two_layers_in_subtree_draw_content) {
    DCHECK(!is_root);
    return RenderSurfaceReason::kOpacity;
  }
  // If the layer has isolation.
  // TODO(rosca): to be optimized - create separate rendering surface only when
  // the blending descendants might have access to the content behind this layer
  // (layer has transparent background or descendants overflow).
  // https://code.google.com/p/chromium/issues/detail?id=301738
  if (layer->is_root_for_isolated_group()) {
    return RenderSurfaceReason::kRootOrIsolatedGroup;
  }

  // If we force it.
  if (layer->force_render_surface_for_testing())
    return RenderSurfaceReason::kTest;

  // If we cache it.
  if (layer->cache_render_surface())
    return RenderSurfaceReason::kCache;

  // If we'll make a copy of the layer's contents.
  if (layer->HasCopyRequest())
    return RenderSurfaceReason::kCopyRequest;

  // If the layer is mirrored.
  if (layer->mirror_count())
    return RenderSurfaceReason::kMirrored;

  return RenderSurfaceReason::kNone;
}

static bool UpdateSubtreeHasCopyRequestRecursive(Layer* layer) {
  bool subtree_has_copy_request = false;
  if (layer->HasCopyRequest())
    subtree_has_copy_request = true;
  for (const scoped_refptr<Layer>& child : layer->children()) {
    subtree_has_copy_request |=
        UpdateSubtreeHasCopyRequestRecursive(child.get());
  }
  layer->SetSubtreeHasCopyRequest(subtree_has_copy_request);
  return subtree_has_copy_request;
}

bool PropertyTreeBuilderContext::AddEffectNodeIfNeeded(
    const DataForRecursion& data_from_ancestor,
    Layer* layer,
    DataForRecursion* data_for_children) const {
  const bool is_root = !layer->parent();
  const bool has_transparency = layer->EffectiveOpacity() != 1.f;
  const bool has_potential_opacity_animation =
      HasPotentialOpacityAnimation(mutator_host_, layer);
  const bool has_potential_filter_animation =
      HasPotentiallyRunningFilterAnimation(mutator_host_, layer);

  data_for_children->animation_axis_aligned_since_render_target &=
      AnimationsPreserveAxisAlignment(mutator_host_, layer);
  data_for_children->compound_transform_since_render_target *=
      layer->transform();
  auto render_surface_reason = ComputeRenderSurfaceReason(
      mutator_host_, layer,
      data_for_children->compound_transform_since_render_target,
      data_for_children->animation_axis_aligned_since_render_target);
  bool should_create_render_surface =
      render_surface_reason != RenderSurfaceReason::kNone;

  bool not_axis_aligned_since_last_clip =
      data_from_ancestor.not_axis_aligned_since_last_clip
          ? true
          : !AnimationsPreserveAxisAlignment(mutator_host_, layer) ||
                !layer->transform().Preserves2dAxisAlignment();
  // A non-axis aligned clip may need a render surface. So, we create an effect
  // node.
  bool has_non_axis_aligned_clip =
      not_axis_aligned_since_last_clip && LayerClipsSubtree(layer);

  bool requires_node =
      is_root || has_transparency || has_potential_opacity_animation ||
      has_potential_filter_animation || has_non_axis_aligned_clip ||
      should_create_render_surface || layer->HasRoundedCorner();

  int parent_id = data_from_ancestor.effect_tree_parent;

  if (!requires_node) {
    layer->SetEffectTreeIndex(parent_id);
    data_for_children->effect_tree_parent = parent_id;
    return false;
  }

  int node_id = effect_tree_.Insert(EffectNode(), parent_id);
  EffectNode* node = effect_tree_.back();

  node->stable_id = layer->id();
  node->opacity = layer->opacity();
  node->blend_mode = layer->blend_mode();
  node->unscaled_mask_target_size = layer->bounds();
  node->cache_render_surface = layer->cache_render_surface();
  node->has_copy_request = layer->HasCopyRequest();
  node->filters = layer->filters();
  node->backdrop_filters = layer->backdrop_filters();
  node->backdrop_filter_bounds = layer->backdrop_filter_bounds();
  node->backdrop_filter_quality = layer->backdrop_filter_quality();
  node->backdrop_mask_element_id = layer->backdrop_mask_element_id();
  node->filters_origin = layer->filters_origin();
  node->trilinear_filtering = layer->trilinear_filtering();
  node->has_potential_opacity_animation = has_potential_opacity_animation;
  node->has_potential_filter_animation = has_potential_filter_animation;
  node->double_sided = layer->double_sided();
  node->subtree_hidden = layer->hide_layer_and_subtree();
  node->is_currently_animating_opacity =
      OpacityIsAnimating(mutator_host_, layer);
  node->is_currently_animating_filter = FilterIsAnimating(mutator_host_, layer);
  node->effect_changed = layer->subtree_property_changed();
  node->subtree_has_copy_request = layer->SubtreeHasCopyRequest();
  node->render_surface_reason = render_surface_reason;
  node->closest_ancestor_with_cached_render_surface_id =
      layer->cache_render_surface()
          ? node_id
          : data_from_ancestor.closest_ancestor_with_cached_render_surface;
  node->closest_ancestor_with_copy_request_id =
      layer->HasCopyRequest()
          ? node_id
          : data_from_ancestor.closest_ancestor_with_copy_request;

  if (layer->mask_layer()) {
    node->mask_layer_id = layer->mask_layer()->id();
    effect_tree_.AddMaskLayerId(node->mask_layer_id);
    node->is_masked = true;
  }

  if (layer->HasRoundedCorner()) {
    // This is currently in the local space of the layer and hence in an invalid
    // space. Once we have the associated transform node for this effect node,
    // we will update this to the transform node's coordinate space.
    node->rounded_corner_bounds = RoundedCornerBounds(layer);
    node->is_fast_rounded_corner = layer->is_fast_rounded_corner();
  }

  if (!is_root) {
    // Having a rounded corner or a render surface, both trigger the creation
    // of a transform node.
    if (should_create_render_surface || layer->HasRoundedCorner()) {
      // In this case, we will create a transform node, so it's safe to use the
      // next available id from the transform tree as this effect node's
      // transform id.
      node->transform_id = transform_tree_.next_available_id();
    }
    node->clip_id = data_from_ancestor.clip_tree_parent;
  } else {
    // The root render surface acts as the unbounded and untransformed
    // surface into which content is drawn. The transform node created
    // from the root layer (which includes device scale factor) and
    // the clip node created from the root layer (which includes
    // viewports) apply to the root render surface's content, but not
    // to the root render surface itself.
    node->transform_id = TransformTree::kRootNodeId;
    node->clip_id = ClipTree::kViewportNodeId;
  }

  data_for_children->closest_ancestor_with_cached_render_surface =
      node->closest_ancestor_with_cached_render_surface_id;
  data_for_children->closest_ancestor_with_copy_request =
      node->closest_ancestor_with_copy_request_id;
  data_for_children->effect_tree_parent = node_id;
  layer->SetEffectTreeIndex(node_id);

  // For animation subsystem purposes, if this layer has a compositor element
  // id, we build a map from that id to this effect node.
  if (layer->element_id()) {
    property_trees_.element_id_to_effect_node_index[layer->element_id()] =
        node_id;
  }

  std::vector<std::unique_ptr<viz::CopyOutputRequest>> layer_copy_requests;
  layer->TakeCopyRequests(&layer_copy_requests);
  for (auto& it : layer_copy_requests) {
    effect_tree_.AddCopyRequest(node_id, std::move(it));
  }
  layer_copy_requests.clear();

  if (should_create_render_surface) {
    data_for_children->compound_transform_since_render_target =
        gfx::Transform();
    data_for_children->animation_axis_aligned_since_render_target = true;
  }
  return should_create_render_surface;
}

bool PropertyTreeBuilderContext::UpdateRenderSurfaceIfNeeded(
    int parent_effect_tree_id,
    DataForRecursion* data_for_children,
    bool subtree_has_rounded_corner,
    bool created_transform_node) const {
  // No effect node was generated for this layer.
  if (parent_effect_tree_id == data_for_children->effect_tree_parent) {
    *data_for_children->subtree_has_rounded_corner = subtree_has_rounded_corner;
    return false;
  }

  EffectNode* effect_node =
      effect_tree_.Node(data_for_children->effect_tree_parent);
  const bool has_rounded_corner = !effect_node->rounded_corner_bounds.IsEmpty();

  // Having a rounded corner should trigger a transform node.
  if (has_rounded_corner)
    DCHECK(created_transform_node);

  // If the subtree has a rounded corner and this node also has a rounded
  // corner, then this node needs to have a render surface to prevent any
  // intersections between the rrects. Since GL renderer can only handle a
  // single rrect per quad at draw time, it would be unable to handle
  // intersections thus resulting in artifacts.
  if (subtree_has_rounded_corner && has_rounded_corner)
    effect_node->render_surface_reason = RenderSurfaceReason::kRoundedCorner;

  // Inform the parent that its subtree has rounded corners if one of the two
  // scenario is true:
  //   - The subtree rooted at this node has a rounded corner and this node
  //     does not have a render surface.
  //   - This node has a rounded corner.
  // The parent may have a rounded corner and would want to create a render
  // surface of its own to prevent blending artifacts due to intersecting
  // rounded corners.
  *data_for_children->subtree_has_rounded_corner =
      (subtree_has_rounded_corner && !effect_node->HasRenderSurface()) ||
      has_rounded_corner;
  return effect_node->HasRenderSurface();
}

void PropertyTreeBuilderContext::AddScrollNodeIfNeeded(
    const DataForRecursion& data_from_ancestor,
    Layer* layer,
    DataForRecursion* data_for_children) const {
  int parent_id = data_from_ancestor.scroll_tree_parent;

  bool is_root = !layer->parent();
  bool scrollable = layer->scrollable();
  bool contains_non_fast_scrollable_region =
      !layer->non_fast_scrollable_region().IsEmpty();
  uint32_t main_thread_scrolling_reasons =
      layer->GetMainThreadScrollingReasons();

  bool scroll_node_uninheritable_criteria =
      is_root || scrollable || contains_non_fast_scrollable_region;
  bool has_different_main_thread_scrolling_reasons =
      main_thread_scrolling_reasons !=
      data_from_ancestor.main_thread_scrolling_reasons;
  bool requires_node =
      scroll_node_uninheritable_criteria ||
      (main_thread_scrolling_reasons !=
           MainThreadScrollingReason::kNotScrollingOnMain &&
       (has_different_main_thread_scrolling_reasons ||
        data_from_ancestor
            .scroll_tree_parent_created_by_uninheritable_criteria));

  int node_id;
  if (!requires_node) {
    node_id = parent_id;
    data_for_children->scroll_tree_parent = node_id;
  } else {
    ScrollNode node;
    node.scrollable = scrollable;
    node.main_thread_scrolling_reasons = main_thread_scrolling_reasons;
    node.scrolls_inner_viewport = layer == inner_viewport_scroll_layer_;
    node.scrolls_outer_viewport = layer == outer_viewport_scroll_layer_;

    if (node.scrolls_inner_viewport &&
        data_from_ancestor.in_subtree_of_page_scale_layer) {
      node.max_scroll_offset_affected_by_page_scale = true;
    }

    node.bounds = layer->bounds();
    node.container_bounds = layer->scroll_container_bounds();
    node.offset_to_transform_parent = layer->offset_to_transform_parent();
    node.should_flatten =
        layer->should_flatten_screen_space_transform_from_property_tree();
    node.user_scrollable_horizontal = layer->GetUserScrollableHorizontal();
    node.user_scrollable_vertical = layer->GetUserScrollableVertical();
    node.element_id = layer->element_id();
    node.transform_id = data_for_children->transform_tree_parent;

    node_id = scroll_tree_.Insert(node, parent_id);
    data_for_children->scroll_tree_parent = node_id;
    data_for_children->main_thread_scrolling_reasons =
        node.main_thread_scrolling_reasons;
    data_for_children->scroll_tree_parent_created_by_uninheritable_criteria =
        scroll_node_uninheritable_criteria;
    // For animation subsystem purposes, if this layer has a compositor element
    // id, we build a map from that id to this scroll node.
    if (layer->element_id()) {
      property_trees_.element_id_to_scroll_node_index[layer->element_id()] =
          node_id;
    }

    if (node.scrollable) {
      scroll_tree_.SetBaseScrollOffset(layer->element_id(),
                                       layer->CurrentScrollOffset());
    }
  }

  layer->SetScrollTreeIndex(node_id);
}

void SetBackfaceVisibilityTransform(Layer* layer, bool created_transform_node) {
  if (layer->use_parent_backface_visibility()) {
    DCHECK(layer->parent());
    DCHECK(!layer->parent()->use_parent_backface_visibility());
    layer->SetShouldCheckBackfaceVisibility(
        layer->parent()->should_check_backface_visibility());
  } else {
    // A double-sided layer's backface can been shown when its visible.
    // In addition, we need to check if (1) there might be a local 3D transform
    // on the layer that might turn it to the backface, or (2) it is not drawn
    // into a flattened space.
    layer->SetShouldCheckBackfaceVisibility(
        !layer->double_sided() &&
        (created_transform_node ||
         !layer->parent()->should_flatten_transform()));
  }
}

void SetSafeOpaqueBackgroundColor(const DataForRecursion& data_from_ancestor,
                                  Layer* layer,
                                  DataForRecursion* data_for_children) {
  SkColor background_color = layer->background_color();
  data_for_children->safe_opaque_background_color =
      SkColorGetA(background_color) == 255
          ? background_color
          : data_from_ancestor.safe_opaque_background_color;
  layer->SetSafeOpaqueBackgroundColor(
      data_for_children->safe_opaque_background_color);
}

void PropertyTreeBuilderContext::BuildPropertyTreesInternal(
    Layer* layer,
    const DataForRecursion& data_from_parent) const {
  layer->set_property_tree_sequence_number(property_trees_.sequence_number);

  DataForRecursion data_for_children(data_from_parent);
  *data_for_children.subtree_has_rounded_corner = false;

  bool created_render_surface =
      AddEffectNodeIfNeeded(data_from_parent, layer, &data_for_children);

  bool created_transform_node = AddTransformNodeIfNeeded(
      data_from_parent, layer, created_render_surface, &data_for_children);
  layer->SetHasTransformNode(created_transform_node);
  AddClipNodeIfNeeded(data_from_parent, layer, created_transform_node,
                      &data_for_children);

  AddScrollNodeIfNeeded(data_from_parent, layer, &data_for_children);

  SetBackfaceVisibilityTransform(layer, created_transform_node);
  SetSafeOpaqueBackgroundColor(data_from_parent, layer, &data_for_children);

  bool not_axis_aligned_since_last_clip =
      data_from_parent.not_axis_aligned_since_last_clip
          ? true
          : !AnimationsPreserveAxisAlignment(mutator_host_, layer) ||
                !layer->transform().Preserves2dAxisAlignment();
  bool has_non_axis_aligned_clip =
      not_axis_aligned_since_last_clip && LayerClipsSubtree(layer);
  data_for_children.not_axis_aligned_since_last_clip =
      !has_non_axis_aligned_clip;

  bool subtree_has_rounded_corner = false;
  for (const scoped_refptr<Layer>& child : layer->children()) {
    if (layer->subtree_property_changed())
      child->SetSubtreePropertyChanged();
    BuildPropertyTreesInternal(child.get(), data_for_children);
    subtree_has_rounded_corner |= *data_for_children.subtree_has_rounded_corner;
  }

  created_render_surface = UpdateRenderSurfaceIfNeeded(
      data_from_parent.effect_tree_parent, &data_for_children,
      subtree_has_rounded_corner, created_transform_node);

  if (layer->mask_layer()) {
    layer->mask_layer()->set_property_tree_sequence_number(
        property_trees_.sequence_number);
    layer->mask_layer()->SetOffsetToTransformParent(
        layer->offset_to_transform_parent());
    layer->mask_layer()->SetTransformTreeIndex(layer->transform_tree_index());
    layer->mask_layer()->SetClipTreeIndex(layer->clip_tree_index());
    layer->mask_layer()->SetEffectTreeIndex(layer->effect_tree_index());
    layer->mask_layer()->SetScrollTreeIndex(layer->scroll_tree_index());
  }
}

void PropertyTreeBuilderContext::BuildPropertyTrees(
    float device_scale_factor,
    const gfx::Rect& viewport,
    SkColor root_background_color) const {
  if (!property_trees_.needs_rebuild) {
    DCHECK_NE(page_scale_layer_, root_layer_);
    if (page_scale_layer_) {
      DCHECK_GE(page_scale_layer_->transform_tree_index(),
                TransformTree::kRootNodeId);
      TransformNode* node = property_trees_.transform_tree.Node(
          page_scale_layer_->transform_tree_index());
      draw_property_utils::UpdatePageScaleFactor(&property_trees_, node,
                                                 page_scale_factor_);
    }
    draw_property_utils::UpdateElasticOverscroll(
        &property_trees_, overscroll_elasticity_element_id_,
        elastic_overscroll_);
    clip_tree_.SetViewportClip(gfx::RectF(viewport));
    // SetRootScaleAndTransform will be incorrect if the root layer has
    // non-zero position, so ensure it is zero.
    DCHECK(root_layer_->position().IsOrigin());
    transform_tree_.SetRootScaleAndTransform(device_scale_factor,
                                             device_transform_);
    return;
  }

  DataForRecursion data_for_recursion;
  data_for_recursion.transform_tree_parent = TransformTree::kInvalidNodeId;
  data_for_recursion.clip_tree_parent = ClipTree::kRootNodeId;
  data_for_recursion.effect_tree_parent = EffectTree::kInvalidNodeId;
  data_for_recursion.scroll_tree_parent = ScrollTree::kRootNodeId;
  data_for_recursion.closest_ancestor_with_cached_render_surface =
      EffectTree::kInvalidNodeId;
  data_for_recursion.closest_ancestor_with_copy_request =
      EffectTree::kInvalidNodeId;
  data_for_recursion.in_subtree_of_page_scale_layer = false;
  data_for_recursion.affected_by_outer_viewport_bounds_delta = false;
  data_for_recursion.should_flatten = false;
  data_for_recursion.main_thread_scrolling_reasons =
      MainThreadScrollingReason::kNotScrollingOnMain;
  data_for_recursion.scroll_tree_parent_created_by_uninheritable_criteria =
      true;
  data_for_recursion.compound_transform_since_render_target = gfx::Transform();
  data_for_recursion.animation_axis_aligned_since_render_target = true;
  data_for_recursion.not_axis_aligned_since_last_clip = false;
  data_for_recursion.safe_opaque_background_color = root_background_color;

  property_trees_.clear();
  transform_tree_.set_device_scale_factor(device_scale_factor);
  ClipNode root_clip;
  root_clip.clip_type = ClipNode::ClipType::APPLIES_LOCAL_CLIP;
  root_clip.clip = gfx::RectF(viewport);
  root_clip.transform_id = TransformTree::kRootNodeId;
  data_for_recursion.clip_tree_parent =
      clip_tree_.Insert(root_clip, ClipTree::kRootNodeId);

  bool subtree_has_rounded_corner;
  data_for_recursion.subtree_has_rounded_corner = &subtree_has_rounded_corner;

  BuildPropertyTreesInternal(root_layer_, data_for_recursion);
  property_trees_.needs_rebuild = false;

  // The transform tree is kept up to date as it is built, but the
  // combined_clips stored in the clip tree and the screen_space_opacity and
  // is_drawn in the effect tree aren't computed during tree building.
  transform_tree_.set_needs_update(false);
  clip_tree_.set_needs_update(true);
  effect_tree_.set_needs_update(true);
  scroll_tree_.set_needs_update(false);
}

}  // namespace

void PropertyTreeBuilder::BuildPropertyTrees(
    Layer* root_layer,
    const Layer* page_scale_layer,
    const Layer* inner_viewport_scroll_layer,
    const Layer* outer_viewport_scroll_layer,
    const ElementId overscroll_elasticity_element_id,
    const gfx::Vector2dF& elastic_overscroll,
    float page_scale_factor,
    float device_scale_factor,
    const gfx::Rect& viewport,
    const gfx::Transform& device_transform,
    PropertyTrees* property_trees) {
  property_trees->is_main_thread = true;
  property_trees->is_active = false;
  SkColor color = root_layer->layer_tree_host()->background_color();
  if (SkColorGetA(color) != 255)
    color = SkColorSetA(color, 255);
  if (root_layer->layer_tree_host()->has_copy_request())
    UpdateSubtreeHasCopyRequestRecursive(root_layer);
  PropertyTreeBuilderContext(
      root_layer, page_scale_layer, inner_viewport_scroll_layer,
      outer_viewport_scroll_layer, overscroll_elasticity_element_id,
      elastic_overscroll, page_scale_factor, device_transform,
      root_layer->layer_tree_host()->mutator_host(), property_trees)
      .BuildPropertyTrees(device_scale_factor, viewport, color);
  property_trees->ResetCachedData();
  // During building property trees, all copy requests are moved from layers to
  // effect tree, which are then pushed at commit to compositor thread and
  // handled there. LayerTreeHost::has_copy_request is only required to
  // decide if we want to create a effect node. So, it can be reset now.
  root_layer->layer_tree_host()->SetHasCopyRequest(false);
}

}  // namespace cc
