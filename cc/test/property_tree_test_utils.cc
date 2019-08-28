// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/property_tree_test_utils.h"

#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/trees/clip_node.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/property_tree.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/transform_node.h"

namespace cc {

namespace {

template <typename LayerType>
void SetupRootPropertiesInternal(LayerType* root) {
  root->set_property_tree_sequence_number(
      GetPropertyTrees(root)->sequence_number);

  auto& root_transform_node =
      CreateTransformNode(root, TransformTree::kRootNodeId);
  DCHECK_EQ(root_transform_node.id, TransformTree::kContentsRootNodeId);

  auto& root_clip_node = CreateClipNode(root, ClipTree::kRootNodeId);
  DCHECK_EQ(root_clip_node.id, ClipTree::kViewportNodeId);
  root_clip_node.clip = gfx::RectF(gfx::SizeF(root->bounds()));

  auto& root_effect_node = CreateEffectNode(root, EffectTree::kRootNodeId);
  DCHECK_EQ(root_effect_node.id, EffectTree::kContentsRootNodeId);
  root_effect_node.render_surface_reason = RenderSurfaceReason::kRoot;

  auto& root_scroll_node = CreateScrollNode(root, ScrollTree::kRootNodeId);
  DCHECK_EQ(root_scroll_node.id, ScrollTree::kSecondaryRootNodeId);
}

template <typename LayerType>
void CopyPropertiesInternal(const LayerType* from, LayerType* to) {
  to->SetTransformTreeIndex(from->transform_tree_index());
  to->SetClipTreeIndex(from->clip_tree_index());
  to->SetEffectTreeIndex(from->effect_tree_index());
  to->SetScrollTreeIndex(from->scroll_tree_index());
}

int ParentId(int parent_id, int default_id) {
  return parent_id == TransformTree::kInvalidNodeId ? default_id : parent_id;
}

template <typename LayerType>
TransformNode& CreateTransformNodeInternal(LayerType* layer, int parent_id) {
  auto* property_trees = GetPropertyTrees(layer);
  auto& transform_tree = property_trees->transform_tree;
  int id = transform_tree.Insert(
      TransformNode(), ParentId(parent_id, layer->transform_tree_index()));
  layer->SetTransformTreeIndex(id);
  layer->SetHasTransformNode(true);
  auto* node = transform_tree.Node(id);
  node->element_id = layer->element_id();
  if (node->element_id) {
    property_trees->element_id_to_transform_node_index[node->element_id] =
        node->id;
  }
  transform_tree.set_needs_update(true);
  return *node;
}

template <typename LayerType>
ClipNode& CreateClipNodeInternal(LayerType* layer, int parent_id) {
  auto& clip_tree = GetPropertyTrees(layer)->clip_tree;
  int id = clip_tree.Insert(ClipNode(),
                            ParentId(parent_id, layer->clip_tree_index()));
  layer->SetClipTreeIndex(id);
  auto* node = clip_tree.Node(id);
  node->clip_type = ClipNode::ClipType::APPLIES_LOCAL_CLIP;
  node->transform_id = layer->transform_tree_index();
  node->clip = gfx::RectF(
      gfx::PointAtOffsetFromOrigin(layer->offset_to_transform_parent()),
      gfx::SizeF(layer->bounds()));
  clip_tree.set_needs_update(true);
  return *node;
}

template <typename LayerType>
EffectNode& CreateEffectNodeInternal(LayerType* layer, int parent_id) {
  auto* property_trees = GetPropertyTrees(layer);
  auto& effect_tree = property_trees->effect_tree;
  int id = effect_tree.Insert(EffectNode(),
                              ParentId(parent_id, layer->effect_tree_index()));
  layer->SetEffectTreeIndex(id);
  auto* node = effect_tree.Node(id);
  node->stable_id = layer->id();
  node->transform_id = layer->transform_tree_index();
  node->clip_id = layer->clip_tree_index();
  if (layer->element_id()) {
    property_trees->element_id_to_effect_node_index[layer->element_id()] =
        node->id;
  }
  effect_tree.set_needs_update(true);
  return *node;
}

template <typename LayerType>
ScrollNode& CreateScrollNodeInternal(LayerType* layer, int parent_id) {
  auto* property_trees = GetPropertyTrees(layer);
  auto& scroll_tree = property_trees->scroll_tree;
  int id = scroll_tree.Insert(ScrollNode(),
                              ParentId(parent_id, layer->scroll_tree_index()));
  layer->SetScrollTreeIndex(id);
  auto* node = scroll_tree.Node(id);
  node->element_id = layer->element_id();
  if (node->element_id) {
    property_trees->element_id_to_scroll_node_index[node->element_id] =
        node->id;
  }
  node->container_bounds = layer->scroll_container_bounds();
  node->bounds = layer->bounds();
  node->scrollable = layer->scrollable();
  node->user_scrollable_horizontal = true;
  node->user_scrollable_vertical = true;

  DCHECK(layer->has_transform_node());
  node->transform_id = layer->transform_tree_index();
  auto* transform_node = GetTransformNode(layer);
  transform_node->should_be_snapped = true;
  transform_node->scrolls = true;

  scroll_tree.set_needs_update(true);
  return *node;
}

template <typename LayerType>
void SetScrollOffsetInternal(LayerType* layer,
                             const gfx::ScrollOffset& scroll_offset) {
  auto* transform_node = GetTransformNode(layer);
  transform_node->scroll_offset = scroll_offset;
  transform_node->needs_local_transform_update = true;
  GetPropertyTrees(layer)->transform_tree.set_needs_update(true);
  GetPropertyTrees(layer)->scroll_tree.SetScrollOffset(layer->element_id(),
                                                       scroll_offset);
}

}  // anonymous namespace

void SetupRootProperties(Layer* root) {
  SetupRootPropertiesInternal(root);
}

void SetupRootProperties(LayerImpl* root) {
  SetupRootPropertiesInternal(root);
}

void CopyProperties(const Layer* from, Layer* to) {
  DCHECK(from->layer_tree_host()->IsUsingLayerLists());
  to->SetLayerTreeHost(from->layer_tree_host());
  to->set_property_tree_sequence_number(from->property_tree_sequence_number());
  CopyPropertiesInternal(from, to);
}

void CopyProperties(const LayerImpl* from, LayerImpl* to) {
  CopyPropertiesInternal(from, to);
}

TransformNode& CreateTransformNode(Layer* layer, int parent_id) {
  DCHECK(layer->layer_tree_host()->IsUsingLayerLists());
  return CreateTransformNodeInternal(layer, parent_id);
}

TransformNode& CreateTransformNode(LayerImpl* layer, int parent_id) {
  return CreateTransformNodeInternal(layer, parent_id);
}

ClipNode& CreateClipNode(Layer* layer, int parent_id) {
  DCHECK(layer->layer_tree_host()->IsUsingLayerLists());
  return CreateClipNodeInternal(layer, parent_id);
}

ClipNode& CreateClipNode(LayerImpl* layer, int parent_id) {
  return CreateClipNodeInternal(layer, parent_id);
}

EffectNode& CreateEffectNode(Layer* layer, int parent_id) {
  DCHECK(layer->layer_tree_host()->IsUsingLayerLists());
  return CreateEffectNodeInternal(layer, parent_id);
}

EffectNode& CreateEffectNode(LayerImpl* layer, int parent_id) {
  return CreateEffectNodeInternal(layer, parent_id);
}

ScrollNode& CreateScrollNode(Layer* layer, int parent_id) {
  DCHECK(layer->layer_tree_host()->IsUsingLayerLists());
  return CreateScrollNodeInternal(layer, parent_id);
}

ScrollNode& CreateScrollNode(LayerImpl* layer, int parent_id) {
  return CreateScrollNodeInternal(layer, parent_id);
}

void SetScrollOffset(Layer* layer, const gfx::ScrollOffset& scroll_offset) {
  layer->SetScrollOffset(scroll_offset);
  SetScrollOffsetInternal(layer, scroll_offset);
}

void SetScrollOffset(LayerImpl* layer, const gfx::ScrollOffset& scroll_offset) {
  layer->SetCurrentScrollOffset(scroll_offset);
  SetScrollOffsetInternal(layer, scroll_offset);
}

void SetupViewport(Layer* root,
                   scoped_refptr<Layer> outer_scroll_layer,
                   const gfx::Size& outer_bounds) {
  DCHECK(root);
  scoped_refptr<Layer> inner_viewport_container_layer = Layer::Create();
  scoped_refptr<Layer> overscroll_elasticity_layer = Layer::Create();
  scoped_refptr<Layer> inner_viewport_scroll_layer = Layer::Create();
  scoped_refptr<Layer> outer_viewport_container_layer = Layer::Create();
  scoped_refptr<Layer> page_scale_layer = Layer::Create();

  inner_viewport_scroll_layer->SetElementId(
      LayerIdToElementIdForTesting(inner_viewport_scroll_layer->id()));
  outer_scroll_layer->SetElementId(
      LayerIdToElementIdForTesting(outer_scroll_layer->id()));
  overscroll_elasticity_layer->SetElementId(
      LayerIdToElementIdForTesting(overscroll_elasticity_layer->id()));

  inner_viewport_container_layer->SetBounds(root->bounds());
  inner_viewport_scroll_layer->SetScrollable(root->bounds());
  inner_viewport_scroll_layer->SetHitTestable(true);
  inner_viewport_scroll_layer->SetBounds(outer_bounds);
  outer_viewport_container_layer->SetBounds(outer_bounds);
  outer_scroll_layer->SetScrollable(outer_bounds);
  outer_scroll_layer->SetHitTestable(true);

  root->AddChild(inner_viewport_container_layer);
  if (root->layer_tree_host()->IsUsingLayerLists()) {
    root->AddChild(overscroll_elasticity_layer);
    root->AddChild(page_scale_layer);
    root->AddChild(inner_viewport_scroll_layer);
    root->AddChild(outer_viewport_container_layer);
    root->AddChild(outer_scroll_layer);

    CopyProperties(root, inner_viewport_container_layer.get());
    CopyProperties(inner_viewport_container_layer.get(),
                   overscroll_elasticity_layer.get());
    CreateTransformNode(overscroll_elasticity_layer.get());
    CopyProperties(overscroll_elasticity_layer.get(), page_scale_layer.get());
    CreateTransformNode(page_scale_layer.get());
    CopyProperties(page_scale_layer.get(), inner_viewport_scroll_layer.get());
    CreateTransformNode(inner_viewport_scroll_layer.get());
    CreateScrollNode(inner_viewport_scroll_layer.get());
    CopyProperties(inner_viewport_scroll_layer.get(),
                   outer_viewport_container_layer.get());
    CopyProperties(outer_viewport_container_layer.get(),
                   outer_scroll_layer.get());
    CreateTransformNode(outer_scroll_layer.get());
    CreateScrollNode(outer_scroll_layer.get());
    // TODO(wangxianzhu): Create other property nodes when they are needed by
    // tests newly converted to layer list mode.
  } else {
    inner_viewport_container_layer->AddChild(overscroll_elasticity_layer);
    overscroll_elasticity_layer->AddChild(page_scale_layer);
    page_scale_layer->AddChild(inner_viewport_scroll_layer);
    inner_viewport_scroll_layer->AddChild(outer_viewport_container_layer);
    outer_viewport_container_layer->AddChild(outer_scroll_layer);
    root->layer_tree_host()->property_trees()->needs_rebuild = true;
  }

  ViewportLayers viewport_layers;
  viewport_layers.overscroll_elasticity_element_id =
      overscroll_elasticity_layer->element_id();
  viewport_layers.page_scale = page_scale_layer;
  viewport_layers.inner_viewport_container = inner_viewport_container_layer;
  viewport_layers.outer_viewport_container = outer_viewport_container_layer;
  viewport_layers.inner_viewport_scroll = inner_viewport_scroll_layer;
  viewport_layers.outer_viewport_scroll = outer_scroll_layer;
  root->layer_tree_host()->RegisterViewportLayers(viewport_layers);
}

void SetupViewport(Layer* root,
                   const gfx::Size& outer_bounds,
                   const gfx::Size& scroll_bounds) {
  scoped_refptr<Layer> outer_viewport_scroll_layer = Layer::Create();
  outer_viewport_scroll_layer->SetBounds(scroll_bounds);
  outer_viewport_scroll_layer->SetIsDrawable(true);
  outer_viewport_scroll_layer->SetHitTestable(true);
  SetupViewport(root, outer_viewport_scroll_layer, outer_bounds);
}

}  // namespace cc
