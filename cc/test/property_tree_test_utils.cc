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
  root->SetElementId(LayerIdToElementIdForTesting(root->id()));

  auto& root_transform_node =
      CreateTransformNode(root, TransformTree::kRootNodeId);
  DCHECK_EQ(root_transform_node.id, TransformTree::kContentsRootNodeId);

  auto& root_clip_node = CreateClipNode(root, ClipTree::kRootNodeId);
  DCHECK_EQ(root_clip_node.id, ClipTree::kViewportNodeId);
  root_clip_node.clip = gfx::RectF(gfx::SizeF(root->bounds()));
  // Root clip is in the real root transform space instead of the root layer's
  // transform space.
  root_clip_node.transform_id = TransformTree::kRootNodeId;

  auto& root_effect_node = CreateEffectNode(root, EffectTree::kRootNodeId);
  DCHECK_EQ(root_effect_node.id, EffectTree::kContentsRootNodeId);
  root_effect_node.render_surface_reason = RenderSurfaceReason::kRoot;
  // Root effect is in the real root transform space instead of the root layer's
  // transform space.
  root_effect_node.transform_id = TransformTree::kRootNodeId;

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
  if (const auto* parent_node = transform_tree.Node(node->parent_id)) {
    node->in_subtree_of_page_scale_layer =
        parent_node->in_subtree_of_page_scale_layer;
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

  scroll_tree.SetScrollOffset(layer->element_id(), gfx::ScrollOffset());
  scroll_tree.set_needs_update(true);
  return *node;
}

template <typename LayerType>
void SetScrollOffsetInternal(LayerType* layer,
                             const gfx::ScrollOffset& scroll_offset) {
  DCHECK(layer->has_transform_node());
  auto* transform_node = GetTransformNode(layer);
  transform_node->scroll_offset = scroll_offset;
  SetLocalTransformChanged(layer);
  GetPropertyTrees(layer)->scroll_tree.SetScrollOffset(layer->element_id(),
                                                       scroll_offset);
}

ElementId OverscrollElasticityElementId() {
  // It is unlikely to conflict with other element ids from layer ids.
  return LayerIdToElementIdForTesting(200000);
}

// TODO(wangxianzhu): Viewport properties can exist without layers, but for now
// it's more convenient to create properties based on layers.
template <typename LayerType>
void SetupViewportProperties(LayerType* root,
                             LayerType* page_scale_layer,
                             LayerType* inner_viewport_container_layer,
                             LayerType* inner_viewport_scroll_layer,
                             LayerType* outer_viewport_container_layer,
                             LayerType* outer_viewport_scroll_layer) {
  CopyProperties(root, inner_viewport_container_layer);

  CopyProperties(inner_viewport_container_layer, page_scale_layer);
  auto& elasticity_transform = CreateTransformNode(page_scale_layer);
  elasticity_transform.element_id = OverscrollElasticityElementId();
  GetPropertyTrees(root)
      ->element_id_to_transform_node_index[elasticity_transform.element_id] =
      elasticity_transform.id;
  CreateTransformNode(page_scale_layer).in_subtree_of_page_scale_layer = true;
  CopyProperties(page_scale_layer, inner_viewport_scroll_layer);

  CreateTransformNode(inner_viewport_scroll_layer);
  auto& inner_scroll_node = CreateScrollNode(inner_viewport_scroll_layer);
  inner_scroll_node.scrolls_inner_viewport = true;
  inner_scroll_node.max_scroll_offset_affected_by_page_scale = true;

  CopyProperties(inner_viewport_scroll_layer, outer_viewport_container_layer);

  CopyProperties(outer_viewport_container_layer, outer_viewport_scroll_layer);
  CreateTransformNode(outer_viewport_scroll_layer);
  CreateScrollNode(outer_viewport_scroll_layer).scrolls_outer_viewport = true;
  // TODO(wangxianzhu): Create other property nodes when they are needed by
  // tests newly converted to layer list mode.
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
  if (layer->IsActive())
    layer->SetCurrentScrollOffset(scroll_offset);
  SetScrollOffsetInternal(layer, scroll_offset);
}

void SetupViewport(Layer* root,
                   scoped_refptr<Layer> outer_viewport_scroll_layer,
                   const gfx::Size& outer_viewport_size) {
  DCHECK(root);
  DCHECK_EQ(root, root->layer_tree_host()->root_layer());
  DCHECK(root->layer_tree_host()->IsUsingLayerLists());

  scoped_refptr<Layer> inner_viewport_container_layer = Layer::Create();
  scoped_refptr<Layer> overscroll_elasticity_layer;
  scoped_refptr<Layer> page_scale_layer = Layer::Create();
  scoped_refptr<Layer> inner_viewport_scroll_layer = Layer::Create();
  scoped_refptr<Layer> outer_viewport_container_layer = Layer::Create();

  inner_viewport_container_layer->SetBounds(root->bounds());
  inner_viewport_scroll_layer->SetBounds(outer_viewport_size);
  inner_viewport_scroll_layer->SetScrollable(root->bounds());
  inner_viewport_scroll_layer->SetHitTestable(true);
  outer_viewport_container_layer->SetBounds(outer_viewport_size);
  outer_viewport_scroll_layer->SetScrollable(outer_viewport_size);
  outer_viewport_scroll_layer->SetHitTestable(true);

  root->AddChild(inner_viewport_container_layer);
  root->AddChild(page_scale_layer);
  root->AddChild(inner_viewport_scroll_layer);
  root->AddChild(outer_viewport_container_layer);
  root->AddChild(outer_viewport_scroll_layer);

  root->layer_tree_host()->SetElementIdsForTesting();
  ViewportLayers viewport_layers;
  viewport_layers.overscroll_elasticity_element_id =
      OverscrollElasticityElementId();
  viewport_layers.page_scale = page_scale_layer;
  viewport_layers.inner_viewport_container = inner_viewport_container_layer;
  viewport_layers.outer_viewport_container = outer_viewport_container_layer;
  viewport_layers.inner_viewport_scroll = inner_viewport_scroll_layer;
  viewport_layers.outer_viewport_scroll = outer_viewport_scroll_layer;
  root->layer_tree_host()->RegisterViewportLayers(viewport_layers);

  SetupViewportProperties(
      root, page_scale_layer.get(), inner_viewport_container_layer.get(),
      inner_viewport_scroll_layer.get(), outer_viewport_container_layer.get(),
      outer_viewport_scroll_layer.get());
}

void SetupViewport(Layer* root,
                   const gfx::Size& outer_viewport_size,
                   const gfx::Size& content_size) {
  scoped_refptr<Layer> outer_viewport_scroll_layer = Layer::Create();
  outer_viewport_scroll_layer->SetBounds(content_size);
  outer_viewport_scroll_layer->SetIsDrawable(true);
  outer_viewport_scroll_layer->SetHitTestable(true);
  SetupViewport(root, outer_viewport_scroll_layer, outer_viewport_size);
}

void SetupViewport(LayerImpl* root,
                   const gfx::Size& outer_viewport_size,
                   const gfx::Size& content_size) {
  DCHECK(root);

  LayerTreeImpl* layer_tree_impl = root->layer_tree_impl();
  DCHECK(!layer_tree_impl->InnerViewportScrollLayer());
  DCHECK(layer_tree_impl->settings().use_layer_lists);
  DCHECK_EQ(root, layer_tree_impl->root_layer_for_testing());

  std::unique_ptr<LayerImpl> inner_viewport_container_layer =
      LayerImpl::Create(layer_tree_impl, 10000);
  std::unique_ptr<LayerImpl> page_scale_layer =
      LayerImpl::Create(layer_tree_impl, 10001);
  std::unique_ptr<LayerImpl> inner_viewport_scroll_layer =
      LayerImpl::Create(layer_tree_impl, 10002);
  std::unique_ptr<LayerImpl> outer_viewport_container_layer =
      LayerImpl::Create(layer_tree_impl, 10003);
  std::unique_ptr<LayerImpl> outer_viewport_scroll_layer =
      LayerImpl::Create(layer_tree_impl, 10004);

  inner_viewport_container_layer->SetBounds(root->bounds());
  inner_viewport_container_layer->SetMasksToBounds(true);
  inner_viewport_scroll_layer->SetBounds(outer_viewport_size);
  inner_viewport_scroll_layer->SetScrollable(root->bounds());
  inner_viewport_scroll_layer->SetHitTestable(true);
  outer_viewport_container_layer->SetBounds(outer_viewport_size);
  outer_viewport_container_layer->SetMasksToBounds(true);
  outer_viewport_scroll_layer->SetBounds(content_size);
  outer_viewport_scroll_layer->SetDrawsContent(true);
  outer_viewport_scroll_layer->SetScrollable(outer_viewport_size);
  outer_viewport_scroll_layer->SetHitTestable(true);

  LayerTreeImpl::ViewportLayerIds viewport_ids;
  viewport_ids.page_scale = page_scale_layer->id();
  viewport_ids.overscroll_elasticity_element_id =
      OverscrollElasticityElementId();
  viewport_ids.inner_viewport_container = inner_viewport_container_layer->id();
  viewport_ids.inner_viewport_scroll = inner_viewport_scroll_layer->id();
  viewport_ids.outer_viewport_container = outer_viewport_container_layer->id();
  viewport_ids.outer_viewport_scroll = outer_viewport_scroll_layer->id();
  layer_tree_impl->SetViewportLayersFromIds(viewport_ids);

  layer_tree_impl->AddLayer(std::move(inner_viewport_container_layer));
  layer_tree_impl->AddLayer(std::move(page_scale_layer));
  layer_tree_impl->AddLayer(std::move(inner_viewport_scroll_layer));
  layer_tree_impl->AddLayer(std::move(outer_viewport_container_layer));
  layer_tree_impl->AddLayer(std::move(outer_viewport_scroll_layer));
  layer_tree_impl->SetElementIdsForTesting();

  SetupViewportProperties(root, layer_tree_impl->PageScaleLayer(),
                          layer_tree_impl->InnerViewportContainerLayer(),
                          layer_tree_impl->InnerViewportScrollLayer(),
                          layer_tree_impl->OuterViewportContainerLayer(),
                          layer_tree_impl->OuterViewportScrollLayer());
}

}  // namespace cc
