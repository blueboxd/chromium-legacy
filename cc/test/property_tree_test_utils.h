// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_PROPERTY_TREE_TEST_UTILS_H_
#define CC_TEST_PROPERTY_TREE_TEST_UTILS_H_

#include "cc/trees/clip_node.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/property_tree.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/transform_node.h"

namespace cc {

class Layer;
class LayerImpl;

// Sets up properties that apply to the root layer.
void SetupRootProperties(Layer* root);
void SetupRootProperties(LayerImpl* root);

// Copies property tree indexes from |from| to |to|. For the |Layer| form, also
// copies |from|'s layer_host_host and property_tree_sequence_number to |to|.
void CopyProperties(const Layer* from, Layer* to);
void CopyProperties(const LayerImpl* from, LayerImpl* to);

// Each of the following methods creates a property node for the layer,
// and sets the new node as the layer's property node of the type.
// The new property node's parent will be |parent_id| if it's specified.
// Otherwise the layer's current property node of the corresponding type will
// be the parent. The latter case is useful to create property nodes after
// CopyProperties() under the copied properties.
TransformNode& CreateTransformNode(
    Layer*,
    int parent_id = TransformTree::kInvalidNodeId);
TransformNode& CreateTransformNode(
    LayerImpl*,
    int parent_id = TransformTree::kInvalidNodeId);
ClipNode& CreateClipNode(Layer*, int parent_id = ClipTree::kInvalidNodeId);
ClipNode& CreateClipNode(LayerImpl*, int parent_id = ClipTree::kInvalidNodeId);
EffectNode& CreateEffectNode(Layer*,
                             int parent_id = EffectTree::kInvalidNodeId);
EffectNode& CreateEffectNode(LayerImpl*,
                             int parent_id = EffectTree::kInvalidNodeId);
ScrollNode& CreateScrollNode(Layer*,
                             int parent_id = ScrollTree::kInvalidNodeId);
ScrollNode& CreateScrollNode(LayerImpl*,
                             int parent_id = ScrollTree::kInvalidNodeId);

// TODO(wangxianzhu): Add functions to create property nodes not based on
// layers when needed.

}  // namespace cc

#endif  // CC_TEST_PROPERTY_TREE_TEST_UTILS_H_
