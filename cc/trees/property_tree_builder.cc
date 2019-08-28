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

template <typename LayerType>
class PropertyTreeBuilderContext {
 public:
  PropertyTreeBuilderContext(LayerType* root_layer,
                             const LayerType* page_scale_layer,
                             const LayerType* inner_viewport_scroll_layer,
                             const LayerType* outer_viewport_scroll_layer,
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
      LayerType* layer,
      const DataForRecursion& data_from_parent) const;

  bool AddTransformNodeIfNeeded(const DataForRecursion& data_from_ancestor,
                                LayerType* layer,
                                bool created_render_surface,
                                DataForRecursion* data_for_children) const;

  void AddClipNodeIfNeeded(const DataForRecursion& data_from_ancestor,
                           LayerType* layer,
                           bool created_transform_node,
                           DataForRecursion* data_for_children) const;

  bool AddEffectNodeIfNeeded(const DataForRecursion& data_from_ancestor,
                             LayerType* layer,
                             DataForRecursion* data_for_children) const;

  void AddScrollNodeIfNeeded(const DataForRecursion& data_from_ancestor,
                             LayerType* layer,
                             DataForRecursion* data_for_children) const;

  bool UpdateRenderSurfaceIfNeeded(int parent_effect_tree_id,
                                   DataForRecursion* data_for_children,
                                   bool subtree_has_rounded_corner,
                                   bool created_transform_node) const;

  LayerType* root_layer_;
  const LayerType* page_scale_layer_;
  const LayerType* inner_viewport_scroll_layer_;
  const LayerType* outer_viewport_scroll_layer_;
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

static LayerImplList& LayerChildren(LayerImpl* layer) {
  return layer->test_properties()->children;
}

static const LayerList& LayerChildren(Layer* layer) {
  return layer->children();
}

static LayerImpl* LayerChildAt(LayerImpl* layer, int index) {
  return layer->test_properties()->children[index];
}

static Layer* LayerChildAt(Layer* layer, int index) {
  return layer->children()[index].get();
}

static bool HasClipRect(Layer* layer) {
  return !layer->clip_rect().IsEmpty();
}

static bool HasClipRect(LayerImpl* layer) {
  return false;
}

static inline const FilterOperations& Filters(Layer* layer) {
  return layer->filters();
}

static inline const FilterOperations& Filters(LayerImpl* layer) {
  return layer->test_properties()->filters;
}

static bool IsFastRoundedCorner(Layer* layer) {
  return layer->is_fast_rounded_corner();
}

static bool IsFastRoundedCorner(LayerImpl* layer) {
  return false;
}

static bool HasRoundedCorner(Layer* layer) {
  return layer->HasRoundedCorner();
}

static bool HasRoundedCorner(LayerImpl* layer) {
  return !layer->test_properties()->rounded_corner_bounds.IsEmpty();
}

static PictureLayer* MaskLayer(Layer* layer) {
  return layer->mask_layer();
}

static LayerImpl* MaskLayer(LayerImpl* layer) {
  return layer->test_properties()->mask_layer;
}

static const gfx::Transform& Transform(Layer* layer) {
  return layer->transform();
}

static const gfx::Transform& Transform(LayerImpl* layer) {
  return layer->test_properties()->transform;
}

static const gfx::PointF& Position(Layer* layer) {
  return layer->position();
}

static const gfx::PointF& Position(LayerImpl* layer) {
  return layer->test_properties()->position;
}

// Methods to query state from the AnimationHost ----------------------
template <typename LayerType>
bool OpacityIsAnimating(const MutatorHost& host, LayerType* layer) {
  return host.IsAnimatingOpacityProperty(layer->element_id(),
                                         layer->GetElementTypeForAnimation());
}

template <typename LayerType>
bool HasPotentiallyRunningOpacityAnimation(const MutatorHost& host,
                                           LayerType* layer) {
  return host.HasPotentiallyRunningOpacityAnimation(
      layer->element_id(), layer->GetElementTypeForAnimation());
}

template <typename LayerType>
bool FilterIsAnimating(const MutatorHost& host, LayerType* layer) {
  return host.IsAnimatingFilterProperty(layer->element_id(),
                                        layer->GetElementTypeForAnimation());
}

template <typename LayerType>
bool HasPotentiallyRunningFilterAnimation(const MutatorHost& host,
                                          LayerType* layer) {
  return host.HasPotentiallyRunningFilterAnimation(
      layer->element_id(), layer->GetElementTypeForAnimation());
}

template <typename LayerType>
bool TransformIsAnimating(const MutatorHost& host, LayerType* layer) {
  return host.IsAnimatingTransformProperty(layer->element_id(),
                                           layer->GetElementTypeForAnimation());
}

template <typename LayerType>
bool HasPotentiallyRunningTransformAnimation(const MutatorHost& host,
                                             LayerType* layer) {
  return host.HasPotentiallyRunningTransformAnimation(
      layer->element_id(), layer->GetElementTypeForAnimation());
}

template <typename LayerType>
void GetAnimationScales(const MutatorHost& host,
                        LayerType* layer,
                        float* maximum_scale,
                        float* starting_scale) {
  return host.GetAnimationScales(layer->element_id(),
                                 layer->GetElementTypeForAnimation(),
                                 maximum_scale, starting_scale);
}

template <typename LayerType>
bool AnimationsPreserveAxisAlignment(const MutatorHost& host,
                                     LayerType* layer) {
  return host.AnimationsPreserveAxisAlignment(layer->element_id());
}

template <typename LayerType>
bool HasAnyAnimationTargetingProperty(const MutatorHost& host,
                                      LayerType* layer,
                                      TargetProperty::Type property) {
  return host.HasAnyAnimationTargetingProperty(layer->element_id(), property);
}

// -------------------------------------------------------------------

template <typename LayerType>
static bool LayerClipsSubtreeToItsBounds(LayerType* layer) {
  return layer->masks_to_bounds() || MaskLayer(layer);
}

template <typename LayerType>
static bool LayerClipsSubtree(LayerType* layer) {
  return LayerClipsSubtreeToItsBounds(layer) || HasRoundedCorner(layer) ||
         HasClipRect(layer);
}

gfx::RectF EffectiveClipRect(Layer* layer) {
  return layer->EffectiveClipRect();
}

gfx::RectF EffectiveClipRect(LayerImpl* layer) {
  return gfx::RectF(gfx::PointF(), gfx::SizeF(layer->bounds()));
}

static gfx::RRectF RoundedCornerBounds(Layer* layer) {
  return gfx::RRectF(EffectiveClipRect(layer), layer->corner_radii());
}

static gfx::RRectF RoundedCornerBounds(LayerImpl* layer) {
  return layer->test_properties()->rounded_corner_bounds;
}

static Layer* LayerParent(Layer* layer) {
  return layer->parent();
}

static LayerImpl* LayerParent(LayerImpl* layer) {
  return layer->test_properties()->parent;
}

static inline int SortingContextId(Layer* layer) {
  return layer->sorting_context_id();
}

static inline int SortingContextId(LayerImpl* layer) {
  return layer->test_properties()->sorting_context_id;
}

static inline bool Is3dSorted(Layer* layer) {
  return layer->sorting_context_id() != 0;
}

static inline bool Is3dSorted(LayerImpl* layer) {
  return layer->test_properties()->sorting_context_id != 0;
}

static inline void SetHasClipNode(Layer* layer, bool val) {
  layer->SetHasClipNode(val);
}

static inline void SetHasClipNode(LayerImpl* layer, bool val) {}

template <typename LayerType>
void PropertyTreeBuilderContext<LayerType>::AddClipNodeIfNeeded(
    const DataForRecursion& data_from_ancestor,
    LayerType* layer,
    bool created_transform_node,
    DataForRecursion* data_for_children) const {
  const int parent_id = data_from_ancestor.clip_tree_parent;

  bool layer_clips_subtree = LayerClipsSubtree(layer);
  bool requires_node =
      layer_clips_subtree || Filters(layer).HasFilterThatMovesPixels();
  if (!requires_node) {
    data_for_children->clip_tree_parent = parent_id;
  } else {
    ClipNode node;
    node.clip = EffectiveClipRect(layer);

    // Move the clip bounds so that it is relative to the transform parent.
    node.clip += layer->offset_to_transform_parent();

    node.transform_id = created_transform_node
                            ? data_for_children->transform_tree_parent
                            : data_from_ancestor.transform_tree_parent;
    if (layer_clips_subtree) {
      node.clip_type = ClipNode::ClipType::APPLIES_LOCAL_CLIP;
    } else {
      DCHECK(Filters(layer).HasFilterThatMovesPixels());
      node.clip_type = ClipNode::ClipType::EXPANDS_CLIP;
      node.clip_expander = ClipExpander(layer->effect_tree_index());
    }
    data_for_children->clip_tree_parent = clip_tree_.Insert(node, parent_id);
  }

  SetHasClipNode(layer, requires_node);
  layer->SetClipTreeIndex(data_for_children->clip_tree_parent);
}

template <typename LayerType>
static inline bool IsAtBoundaryOf3dRenderingContext(LayerType* layer) {
  return LayerParent(layer)
             ? SortingContextId(LayerParent(layer)) != SortingContextId(layer)
             : Is3dSorted(layer);
}

static inline gfx::Point3F TransformOrigin(Layer* layer) {
  return layer->transform_origin();
}

static inline gfx::Point3F TransformOrigin(LayerImpl* layer) {
  return layer->test_properties()->transform_origin;
}

static inline bool ShouldFlattenTransform(Layer* layer) {
  return layer->should_flatten_transform();
}

static inline bool ShouldFlattenTransform(LayerImpl* layer) {
  return layer->test_properties()->should_flatten_transform;
}

template <typename LayerType>
bool PropertyTreeBuilderContext<LayerType>::AddTransformNodeIfNeeded(
    const DataForRecursion& data_from_ancestor,
    LayerType* layer,
    bool created_render_surface,
    DataForRecursion* data_for_children) const {
  const bool is_root = !LayerParent(layer);
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
      !Transform(layer).IsIdentityOr2DTranslation();

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
                       HasRoundedCorner(layer);

  int parent_index = TransformTree::kRootNodeId;
  gfx::Vector2dF parent_offset;
  if (!is_root) {
    parent_index = data_from_ancestor.transform_tree_parent;
    // Now layer tree mode (IsUsingLayerLists is false) is for ui compositor
    // only. The transform tree hierarchy is always the same as layer hierarchy.
    DCHECK_EQ(parent_index, LayerParent(layer)->transform_tree_index());
    parent_offset = LayerParent(layer)->offset_to_transform_parent();
  }

  if (!requires_node) {
    data_for_children->should_flatten |= ShouldFlattenTransform(layer);
    gfx::Vector2dF local_offset =
        Position(layer).OffsetFromOrigin() + Transform(layer).To2dTranslation();
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
  node->sorting_context_id = SortingContextId(layer);

  if (is_root || is_page_scale_layer) {
    // Root layer and page scale layer should not have transform or offset.
    DCHECK(Position(layer).IsOrigin());
    DCHECK(parent_offset.IsZero());
    DCHECK(Transform(layer).IsIdentity());

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
    node->local = Transform(layer);
    node->origin = TransformOrigin(layer);
    node->post_translation = parent_offset + Position(layer).OffsetFromOrigin();
  }

  node->in_subtree_of_page_scale_layer =
      data_for_children->in_subtree_of_page_scale_layer;

  // Surfaces inherently flatten transforms.
  data_for_children->should_flatten =
      ShouldFlattenTransform(layer) || has_surface;

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

static inline bool HasPotentialOpacityAnimation(const MutatorHost& host,
                                                Layer* layer) {
  return HasPotentiallyRunningOpacityAnimation(host, layer) ||
         layer->OpacityCanAnimateOnImplThread();
}

static inline bool HasPotentialOpacityAnimation(const MutatorHost& host,
                                                LayerImpl* layer) {
  return HasPotentiallyRunningOpacityAnimation(host, layer) ||
         layer->test_properties()->opacity_can_animate;
}

static inline bool DoubleSided(Layer* layer) {
  return layer->double_sided();
}

static inline bool DoubleSided(LayerImpl* layer) {
  return layer->test_properties()->double_sided;
}

static inline bool TrilinearFiltering(Layer* layer) {
  return layer->trilinear_filtering();
}

static inline bool TrilinearFiltering(LayerImpl* layer) {
  return layer->test_properties()->trilinear_filtering;
}

static inline bool CacheRenderSurface(Layer* layer) {
  return layer->cache_render_surface();
}

static inline bool CacheRenderSurface(LayerImpl* layer) {
  return layer->test_properties()->cache_render_surface;
}

static inline bool ForceRenderSurfaceForTesting(Layer* layer) {
  return layer->force_render_surface_for_testing();
}

static inline bool ForceRenderSurfaceForTesting(LayerImpl* layer) {
  return layer->test_properties()->force_render_surface;
}

template <typename LayerType>
static inline bool LayerIsInExisting3DRenderingContext(LayerType* layer) {
  return Is3dSorted(layer) && LayerParent(layer) &&
         Is3dSorted(LayerParent(layer)) &&
         (SortingContextId(LayerParent(layer)) == SortingContextId(layer));
}

static inline bool IsRootForIsolatedGroup(Layer* layer) {
  return layer->is_root_for_isolated_group();
}

static inline bool IsRootForIsolatedGroup(LayerImpl* layer) {
  return false;
}

static inline int NumDescendantsThatDrawContent(Layer* layer) {
  return layer->NumDescendantsThatDrawContent();
}

static inline int NumLayerOrDescendantsThatDrawContentRecursive(
    LayerImpl* layer) {
  int num = layer->DrawsContent() ? 1 : 0;
  for (size_t i = 0; i < layer->test_properties()->children.size(); ++i) {
    LayerImpl* child_layer = layer->test_properties()->children[i];
    num += NumLayerOrDescendantsThatDrawContentRecursive(child_layer);
  }
  return num;
}

static inline int NumDescendantsThatDrawContent(LayerImpl* layer) {
  int num_descendants_that_draw_content = 0;
  for (size_t i = 0; i < layer->test_properties()->children.size(); ++i) {
    LayerImpl* child_layer = layer->test_properties()->children[i];
    num_descendants_that_draw_content +=
        NumLayerOrDescendantsThatDrawContentRecursive(child_layer);
  }
  return num_descendants_that_draw_content;
}

static inline float EffectiveOpacity(Layer* layer) {
  return layer->EffectiveOpacity();
}

static inline float EffectiveOpacity(LayerImpl* layer) {
  return layer->test_properties()->hide_layer_and_subtree
             ? 0.f
             : layer->test_properties()->opacity;
}

static inline float Opacity(Layer* layer) {
  return layer->opacity();
}

static inline float Opacity(LayerImpl* layer) {
  return layer->test_properties()->opacity;
}

static inline SkBlendMode BlendMode(Layer* layer) {
  return layer->blend_mode();
}

static inline SkBlendMode BlendMode(LayerImpl* layer) {
  return layer->test_properties()->blend_mode;
}

static inline const gfx::PointF FiltersOrigin(Layer* layer) {
  return layer->filters_origin();
}

static inline const gfx::PointF FiltersOrigin(LayerImpl* layer) {
  return layer->test_properties()->filters_origin;
}

static inline const FilterOperations& BackdropFilters(Layer* layer) {
  return layer->backdrop_filters();
}

static inline const FilterOperations& BackdropFilters(LayerImpl* layer) {
  return layer->test_properties()->backdrop_filters;
}

static inline const base::Optional<gfx::RRectF>& BackdropFilterBounds(
    Layer* layer) {
  return layer->backdrop_filter_bounds();
}

static inline const base::Optional<gfx::RRectF>& BackdropFilterBounds(
    LayerImpl* layer) {
  return layer->test_properties()->backdrop_filter_bounds;
}

static inline ElementId BackdropMaskElementId(Layer* layer) {
  return layer->backdrop_mask_element_id();
}

static inline ElementId BackdropMaskElementId(LayerImpl* layer) {
  return layer->test_properties()->backdrop_mask_element_id;
}

static inline float BackdropFilterQuality(Layer* layer) {
  return layer->backdrop_filter_quality();
}

static inline float BackdropFilterQuality(LayerImpl* layer) {
  return layer->test_properties()->backdrop_filter_quality;
}

static inline bool HideLayerAndSubtree(Layer* layer) {
  return layer->hide_layer_and_subtree();
}

static inline bool HideLayerAndSubtree(LayerImpl* layer) {
  return layer->test_properties()->hide_layer_and_subtree;
}

static inline bool HasCopyRequest(Layer* layer) {
  return layer->HasCopyRequest();
}

static inline bool HasCopyRequest(LayerImpl* layer) {
  return !layer->test_properties()->copy_requests.empty();
}

static inline int MirrorCount(Layer* layer) {
  return layer->mirror_count();
}

static inline int MirrorCount(LayerImpl* layer) {
  return 0;
}

static inline bool PropertyChanged(Layer* layer) {
  return layer->subtree_property_changed();
}

static inline bool PropertyChanged(LayerImpl* layer) {
  return false;
}

template <typename LayerType>
RenderSurfaceReason ComputeRenderSurfaceReason(const MutatorHost& mutator_host,
                                               LayerType* layer,
                                               gfx::Transform current_transform,
                                               bool animation_axis_aligned) {
  const bool preserves_2d_axis_alignment =
      current_transform.Preserves2dAxisAlignment() && animation_axis_aligned;
  const bool is_root = !LayerParent(layer);
  if (is_root)
    return RenderSurfaceReason::kRoot;

  // If the layer uses a mask.
  if (MaskLayer(layer)) {
    return RenderSurfaceReason::kMask;
  }

  // If the layer uses trilinear filtering.
  if (TrilinearFiltering(layer)) {
    return RenderSurfaceReason::kTrilinearFiltering;
  }

  // If the layer uses a CSS filter.
  if (!Filters(layer).IsEmpty()) {
    return RenderSurfaceReason::kFilter;
  }

  // If the layer uses a CSS backdrop-filter.
  if (!BackdropFilters(layer).IsEmpty()) {
    return RenderSurfaceReason::kBackdropFilter;
  }

  // If the layer will use a CSS filter.  In this case, the animation
  // will start and add a filter to this layer, so it needs a surface.
  if (HasPotentiallyRunningFilterAnimation(mutator_host, layer)) {
    return RenderSurfaceReason::kFilterAnimation;
  }

  int num_descendants_that_draw_content = NumDescendantsThatDrawContent(layer);

  // If the layer flattens its subtree, but it is treated as a 3D object by its
  // parent (i.e. parent participates in a 3D rendering context).
  if (LayerIsInExisting3DRenderingContext(layer) &&
      ShouldFlattenTransform(layer) && num_descendants_that_draw_content > 0) {
    return RenderSurfaceReason::k3dTransformFlattening;
  }

  if (!IsFastRoundedCorner(layer) && HasRoundedCorner(layer) &&
      num_descendants_that_draw_content > 1) {
    return RenderSurfaceReason::kRoundedCorner;
  }

  // If the layer has blending.
  // TODO(rosca): this is temporary, until blending is implemented for other
  // types of quads than viz::RenderPassDrawQuad. Layers having descendants that
  // draw content will still create a separate rendering surface.
  if (BlendMode(layer) != SkBlendMode::kSrcOver) {
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
      EffectiveOpacity(layer) != 1.f ||
      HasPotentiallyRunningOpacityAnimation(mutator_host, layer);
  if (may_have_transparency && ShouldFlattenTransform(layer) &&
      at_least_two_layers_in_subtree_draw_content) {
    DCHECK(!is_root);
    return RenderSurfaceReason::kOpacity;
  }
  // If the layer has isolation.
  // TODO(rosca): to be optimized - create separate rendering surface only when
  // the blending descendants might have access to the content behind this layer
  // (layer has transparent background or descendants overflow).
  // https://code.google.com/p/chromium/issues/detail?id=301738
  if (IsRootForIsolatedGroup(layer)) {
    return RenderSurfaceReason::kRootOrIsolatedGroup;
  }

  // If we force it.
  if (ForceRenderSurfaceForTesting(layer))
    return RenderSurfaceReason::kTest;

  // If we cache it.
  if (CacheRenderSurface(layer))
    return RenderSurfaceReason::kCache;

  // If we'll make a copy of the layer's contents.
  if (HasCopyRequest(layer))
    return RenderSurfaceReason::kCopyRequest;

  // If the layer is mirrored.
  if (MirrorCount(layer))
    return RenderSurfaceReason::kMirrored;

  return RenderSurfaceReason::kNone;
}

static void TakeCopyRequests(
    Layer* layer,
    std::vector<std::unique_ptr<viz::CopyOutputRequest>>* copy_requests) {
  layer->TakeCopyRequests(copy_requests);
}

static void TakeCopyRequests(
    LayerImpl* layer,
    std::vector<std::unique_ptr<viz::CopyOutputRequest>>* copy_requests) {
  for (auto& request : layer->test_properties()->copy_requests)
    copy_requests->push_back(std::move(request));
  layer->test_properties()->copy_requests.clear();
}

static void SetSubtreeHasCopyRequest(Layer* layer,
                                     bool subtree_has_copy_request) {
  layer->SetSubtreeHasCopyRequest(subtree_has_copy_request);
}

static void SetSubtreeHasCopyRequest(LayerImpl* layer,
                                     bool subtree_has_copy_request) {
  layer->test_properties()->subtree_has_copy_request = subtree_has_copy_request;
}

static bool SubtreeHasCopyRequest(Layer* layer) {
  return layer->SubtreeHasCopyRequest();
}

static bool SubtreeHasCopyRequest(LayerImpl* layer) {
  return layer->test_properties()->subtree_has_copy_request;
}

template <typename LayerType>
bool UpdateSubtreeHasCopyRequestRecursive(LayerType* layer) {
  bool subtree_has_copy_request = false;
  if (HasCopyRequest(layer))
    subtree_has_copy_request = true;
  for (size_t i = 0; i < LayerChildren(layer).size(); ++i) {
    LayerType* current_child = LayerChildAt(layer, i);
    subtree_has_copy_request |=
        UpdateSubtreeHasCopyRequestRecursive(current_child);
  }
  SetSubtreeHasCopyRequest(layer, subtree_has_copy_request);
  return subtree_has_copy_request;
}

template <typename LayerType>
bool PropertyTreeBuilderContext<LayerType>::AddEffectNodeIfNeeded(
    const DataForRecursion& data_from_ancestor,
    LayerType* layer,
    DataForRecursion* data_for_children) const {
  const bool is_root = !LayerParent(layer);
  const bool has_transparency = EffectiveOpacity(layer) != 1.f;
  const bool has_potential_opacity_animation =
      HasPotentialOpacityAnimation(mutator_host_, layer);
  const bool has_potential_filter_animation =
      HasPotentiallyRunningFilterAnimation(mutator_host_, layer);

  data_for_children->animation_axis_aligned_since_render_target &=
      AnimationsPreserveAxisAlignment(mutator_host_, layer);
  data_for_children->compound_transform_since_render_target *= Transform(layer);
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
                !Transform(layer).Preserves2dAxisAlignment();
  // A non-axis aligned clip may need a render surface. So, we create an effect
  // node.
  bool has_non_axis_aligned_clip =
      not_axis_aligned_since_last_clip && LayerClipsSubtree(layer);

  bool requires_node =
      is_root || has_transparency || has_potential_opacity_animation ||
      has_potential_filter_animation || has_non_axis_aligned_clip ||
      should_create_render_surface || HasRoundedCorner(layer);

  int parent_id = data_from_ancestor.effect_tree_parent;

  if (!requires_node) {
    layer->SetEffectTreeIndex(parent_id);
    data_for_children->effect_tree_parent = parent_id;
    return false;
  }

  int node_id = effect_tree_.Insert(EffectNode(), parent_id);
  EffectNode* node = effect_tree_.back();

  node->stable_id = layer->id();
  node->opacity = Opacity(layer);
  node->blend_mode = BlendMode(layer);
  node->unscaled_mask_target_size = layer->bounds();
  node->cache_render_surface = CacheRenderSurface(layer);
  node->has_copy_request = HasCopyRequest(layer);
  node->filters = Filters(layer);
  node->backdrop_filters = BackdropFilters(layer);
  node->backdrop_filter_bounds = BackdropFilterBounds(layer);
  node->backdrop_filter_quality = BackdropFilterQuality(layer);
  node->backdrop_mask_element_id = BackdropMaskElementId(layer);
  node->filters_origin = FiltersOrigin(layer);
  node->trilinear_filtering = TrilinearFiltering(layer);
  node->has_potential_opacity_animation = has_potential_opacity_animation;
  node->has_potential_filter_animation = has_potential_filter_animation;
  node->double_sided = DoubleSided(layer);
  node->subtree_hidden = HideLayerAndSubtree(layer);
  node->is_currently_animating_opacity =
      OpacityIsAnimating(mutator_host_, layer);
  node->is_currently_animating_filter = FilterIsAnimating(mutator_host_, layer);
  node->effect_changed = PropertyChanged(layer);
  node->subtree_has_copy_request = SubtreeHasCopyRequest(layer);
  node->render_surface_reason = render_surface_reason;
  node->closest_ancestor_with_cached_render_surface_id =
      CacheRenderSurface(layer)
          ? node_id
          : data_from_ancestor.closest_ancestor_with_cached_render_surface;
  node->closest_ancestor_with_copy_request_id =
      HasCopyRequest(layer)
          ? node_id
          : data_from_ancestor.closest_ancestor_with_copy_request;

  if (MaskLayer(layer)) {
    node->mask_layer_id = MaskLayer(layer)->id();
    effect_tree_.AddMaskLayerId(node->mask_layer_id);
    node->is_masked = true;
  }

  if (HasRoundedCorner(layer)) {
    // This is currently in the local space of the layer and hence in an invalid
    // space. Once we have the associated transform node for this effect node,
    // we will update this to the transform node's coordinate space.
    node->rounded_corner_bounds = RoundedCornerBounds(layer);
    node->is_fast_rounded_corner = IsFastRoundedCorner(layer);
  }

  if (!is_root) {
    // Having a rounded corner or a render surface, both trigger the creation
    // of a transform node.
    if (should_create_render_surface || HasRoundedCorner(layer)) {
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
  TakeCopyRequests(layer, &layer_copy_requests);
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

template <typename LayerType>
bool PropertyTreeBuilderContext<LayerType>::UpdateRenderSurfaceIfNeeded(
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

static inline bool UserScrollableHorizontal(Layer* layer) {
  return layer->GetUserScrollableHorizontal();
}

static inline bool UserScrollableHorizontal(LayerImpl* layer) {
  return layer->test_properties()->user_scrollable_horizontal;
}

static inline bool UserScrollableVertical(Layer* layer) {
  return layer->GetUserScrollableVertical();
}

static inline bool UserScrollableVertical(LayerImpl* layer) {
  return layer->test_properties()->user_scrollable_vertical;
}

static inline const base::Optional<SnapContainerData>& GetSnapContainerData(
    Layer* layer) {
  return layer->snap_container_data();
}

static inline const base::Optional<SnapContainerData>& GetSnapContainerData(
    LayerImpl* layer) {
  return layer->test_properties()->snap_container_data;
}

static inline uint32_t MainThreadScrollingReasons(Layer* layer) {
  return layer->GetMainThreadScrollingReasons();
}

static inline uint32_t MainThreadScrollingReasons(LayerImpl* layer) {
  return layer->test_properties()->main_thread_scrolling_reasons;
}

template <typename LayerType>
void SetHasTransformNode(LayerType* layer, bool val) {
  layer->SetHasTransformNode(val);
}

template <typename LayerType>
void PropertyTreeBuilderContext<LayerType>::AddScrollNodeIfNeeded(
    const DataForRecursion& data_from_ancestor,
    LayerType* layer,
    DataForRecursion* data_for_children) const {
  int parent_id = data_from_ancestor.scroll_tree_parent;

  bool is_root = !LayerParent(layer);
  bool scrollable = layer->scrollable();
  bool contains_non_fast_scrollable_region =
      !layer->non_fast_scrollable_region().IsEmpty();
  uint32_t main_thread_scrolling_reasons = MainThreadScrollingReasons(layer);

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
    node.user_scrollable_horizontal = UserScrollableHorizontal(layer);
    node.user_scrollable_vertical = UserScrollableVertical(layer);
    node.element_id = layer->element_id();
    node.transform_id = data_for_children->transform_tree_parent;
    node.snap_container_data = GetSnapContainerData(layer);

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

template <typename LayerType>
void SetBackfaceVisibilityTransform(LayerType* layer,
                                    bool created_transform_node) {
  if (layer->use_parent_backface_visibility()) {
    DCHECK(LayerParent(layer));
    DCHECK(!LayerParent(layer)->use_parent_backface_visibility());
    layer->SetShouldCheckBackfaceVisibility(
        LayerParent(layer)->should_check_backface_visibility());
  } else {
    // A double-sided layer's backface can been shown when its visible.
    // In addition, we need to check if (1) there might be a local 3D transform
    // on the layer that might turn it to the backface, or (2) it is not drawn
    // into a flattened space.
    layer->SetShouldCheckBackfaceVisibility(
        !DoubleSided(layer) && (created_transform_node ||
                                !ShouldFlattenTransform(LayerParent(layer))));
  }
}

template <typename LayerType>
void SetSafeOpaqueBackgroundColor(const DataForRecursion& data_from_ancestor,
                                  LayerType* layer,
                                  DataForRecursion* data_for_children) {
  SkColor background_color = layer->background_color();
  data_for_children->safe_opaque_background_color =
      SkColorGetA(background_color) == 255
          ? background_color
          : data_from_ancestor.safe_opaque_background_color;
  layer->SetSafeOpaqueBackgroundColor(
      data_for_children->safe_opaque_background_color);
}

static void SetLayerPropertyChangedForChild(Layer* parent, Layer* child) {
  if (parent->subtree_property_changed())
    child->SetSubtreePropertyChanged();
}

static void SetLayerPropertyChangedForChild(LayerImpl* parent,
                                            LayerImpl* child) {}

template <typename LayerType>
void PropertyTreeBuilderContext<LayerType>::BuildPropertyTreesInternal(
    LayerType* layer,
    const DataForRecursion& data_from_parent) const {
  layer->set_property_tree_sequence_number(property_trees_.sequence_number);

  DataForRecursion data_for_children(data_from_parent);
  *data_for_children.subtree_has_rounded_corner = false;

  bool created_render_surface =
      AddEffectNodeIfNeeded(data_from_parent, layer, &data_for_children);

  bool created_transform_node = AddTransformNodeIfNeeded(
      data_from_parent, layer, created_render_surface, &data_for_children);
  SetHasTransformNode(layer, created_transform_node);
  AddClipNodeIfNeeded(data_from_parent, layer, created_transform_node,
                      &data_for_children);

  AddScrollNodeIfNeeded(data_from_parent, layer, &data_for_children);

  SetBackfaceVisibilityTransform(layer, created_transform_node);
  SetSafeOpaqueBackgroundColor(data_from_parent, layer, &data_for_children);

  bool not_axis_aligned_since_last_clip =
      data_from_parent.not_axis_aligned_since_last_clip
          ? true
          : !AnimationsPreserveAxisAlignment(mutator_host_, layer) ||
                !Transform(layer).Preserves2dAxisAlignment();
  bool has_non_axis_aligned_clip =
      not_axis_aligned_since_last_clip && LayerClipsSubtree(layer);
  data_for_children.not_axis_aligned_since_last_clip =
      !has_non_axis_aligned_clip;

  bool subtree_has_rounded_corner = false;
  for (size_t i = 0; i < LayerChildren(layer).size(); ++i) {
    LayerType* current_child = LayerChildAt(layer, i);
    SetLayerPropertyChangedForChild(layer, current_child);
    BuildPropertyTreesInternal(current_child, data_for_children);
    subtree_has_rounded_corner |= *data_for_children.subtree_has_rounded_corner;
  }

  created_render_surface = UpdateRenderSurfaceIfNeeded(
      data_from_parent.effect_tree_parent, &data_for_children,
      subtree_has_rounded_corner, created_transform_node);

  if (MaskLayer(layer)) {
    MaskLayer(layer)->set_property_tree_sequence_number(
        property_trees_.sequence_number);
    MaskLayer(layer)->SetOffsetToTransformParent(
        layer->offset_to_transform_parent());
    MaskLayer(layer)->SetTransformTreeIndex(layer->transform_tree_index());
    MaskLayer(layer)->SetClipTreeIndex(layer->clip_tree_index());
    MaskLayer(layer)->SetEffectTreeIndex(layer->effect_tree_index());
    MaskLayer(layer)->SetScrollTreeIndex(layer->scroll_tree_index());
  }
}

}  // namespace

Layer* PropertyTreeBuilder::FindFirstScrollableLayer(Layer* layer) {
  if (!layer)
    return nullptr;

  if (layer->scrollable())
    return layer;

  for (size_t i = 0; i < layer->children().size(); ++i) {
    Layer* found = FindFirstScrollableLayer(layer->children()[i].get());
    if (found)
      return found;
  }

  return nullptr;
}

template <typename LayerType>
void PropertyTreeBuilderContext<LayerType>::BuildPropertyTrees(
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
    DCHECK(Position(root_layer_).IsOrigin());
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
  PropertyTreeBuilderContext<Layer>(
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

void PropertyTreeBuilder::BuildPropertyTrees(
    LayerImpl* root_layer,
    const LayerImpl* page_scale_layer,
    const LayerImpl* inner_viewport_scroll_layer,
    const LayerImpl* outer_viewport_scroll_layer,
    const ElementId overscroll_elasticity_element_id,
    const gfx::Vector2dF& elastic_overscroll,
    float page_scale_factor,
    float device_scale_factor,
    const gfx::Rect& viewport,
    const gfx::Transform& device_transform,
    PropertyTrees* property_trees) {
  // Preserve render surfaces when rebuilding.
  std::vector<std::unique_ptr<RenderSurfaceImpl>> render_surfaces;
  property_trees->effect_tree.TakeRenderSurfaces(&render_surfaces);
  property_trees->is_main_thread = false;
  property_trees->is_active = root_layer->IsActive();
  SkColor color = root_layer->layer_tree_impl()->background_color();
  if (SkColorGetA(color) != 255)
    color = SkColorSetA(color, 255);
  UpdateSubtreeHasCopyRequestRecursive(root_layer);

  PropertyTreeBuilderContext<LayerImpl>(
      root_layer, page_scale_layer, inner_viewport_scroll_layer,
      outer_viewport_scroll_layer, overscroll_elasticity_element_id,
      elastic_overscroll, page_scale_factor, device_transform,
      root_layer->layer_tree_impl()->mutator_host(), property_trees)
      .BuildPropertyTrees(device_scale_factor, viewport, color);
  property_trees->effect_tree.CreateOrReuseRenderSurfaces(
      &render_surfaces, root_layer->layer_tree_impl());
  property_trees->ResetCachedData();
}

}  // namespace cc
