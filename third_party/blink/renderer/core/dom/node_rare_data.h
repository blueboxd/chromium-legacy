/*
 * Copyright (C) 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2008 David Smith <catfish.man@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NODE_RARE_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NODE_RARE_DATA_H_

#include "base/macros.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

class ComputedStyle;
enum class DynamicRestyleFlags;
enum class ElementFlags;
class FlatTreeNodeData;
class LayoutObject;
class MutationObserverRegistration;
class NodeListsNodeData;

class NodeMutationObserverData final
    : public GarbageCollected<NodeMutationObserverData> {
 public:
  NodeMutationObserverData() = default;

  const HeapVector<Member<MutationObserverRegistration>>& Registry() {
    return registry_;
  }

  const HeapHashSet<Member<MutationObserverRegistration>>& TransientRegistry() {
    return transient_registry_;
  }

  void AddTransientRegistration(MutationObserverRegistration* registration);
  void RemoveTransientRegistration(MutationObserverRegistration* registration);
  void AddRegistration(MutationObserverRegistration* registration);
  void RemoveRegistration(MutationObserverRegistration* registration);

  void Trace(Visitor* visitor);

 private:
  HeapVector<Member<MutationObserverRegistration>> registry_;
  HeapHashSet<Member<MutationObserverRegistration>> transient_registry_;
  DISALLOW_COPY_AND_ASSIGN(NodeMutationObserverData);
};

class GC_PLUGIN_IGNORE(
    "GC plugin reports that TraceAfterDispatch is not called but it is called "
    "by both NodeRareDate::TraceAfterDispatch and "
    "NodeRenderingData::TraceAfterDispatch.") NodeData
    : public GarbageCollected<NodeData> {
 public:
  NodeData(bool is_rare_data)
      : connected_frame_count_(0),
        element_flags_(0),
        restyle_flags_(0),
        is_element_rare_data_(false),
        is_rare_data_(is_rare_data) {}
  void Trace(Visitor*);
  void TraceAfterDispatch(blink::Visitor*) {}

  enum {
    kConnectedFrameCountBits = 10,  // Must fit Page::maxNumberOfFrames.
    kNumberOfElementFlags = 6,
    kNumberOfDynamicRestyleFlags = 14
  };

 protected:
  // The top 4 fields belong to NodeRareData. They are located here to conserve
  // space and avoid increase in size of NodeRareData (without locating the
  // fields here, is_rare_data_ will be padded thus increasing the size of
  // NodeRareData by 8 bytes).
  unsigned connected_frame_count_ : kConnectedFrameCountBits;
  unsigned element_flags_ : kNumberOfElementFlags;
  unsigned restyle_flags_ : kNumberOfDynamicRestyleFlags;
  unsigned is_element_rare_data_ : 1;
  unsigned is_rare_data_ : 1;
};

class GC_PLUGIN_IGNORE("Manual dispatch implemented in NodeData.")
    NodeRenderingData final : public NodeData {
 public:
  NodeRenderingData(LayoutObject*,
                    scoped_refptr<const ComputedStyle> computed_style);

  LayoutObject* GetLayoutObject() const { return layout_object_; }
  void SetLayoutObject(LayoutObject* layout_object) {
    DCHECK_NE(&SharedEmptyData(), this);
    layout_object_ = layout_object;
  }

  const ComputedStyle* GetComputedStyle() const {
    return computed_style_.get();
  }
  void SetComputedStyle(scoped_refptr<const ComputedStyle> computed_style);

  static NodeRenderingData& SharedEmptyData();
  bool IsSharedEmptyData() { return this == &SharedEmptyData(); }

  void TraceAfterDispatch(Visitor* visitor) {
    NodeData::TraceAfterDispatch(visitor);
  }

 private:
  LayoutObject* layout_object_;
  scoped_refptr<const ComputedStyle> computed_style_;
  DISALLOW_COPY_AND_ASSIGN(NodeRenderingData);
};

class GC_PLUGIN_IGNORE("Manual dispatch implemented in NodeData.") NodeRareData
    : public NodeData {
 public:
  explicit NodeRareData(NodeRenderingData* node_layout_data)
      : NodeData(true), node_layout_data_(node_layout_data) {
    CHECK_NE(node_layout_data, nullptr);
  }

  NodeRenderingData* GetNodeRenderingData() const { return node_layout_data_; }
  void SetNodeRenderingData(NodeRenderingData* node_layout_data) {
    DCHECK(node_layout_data);
    node_layout_data_ = node_layout_data;
  }

  void ClearNodeLists() { node_lists_.Clear(); }
  NodeListsNodeData* NodeLists() const { return node_lists_.Get(); }
  // EnsureNodeLists() and a following NodeListsNodeData functions must be
  // wrapped with a ThreadState::GCForbiddenScope in order to avoid an
  // initialized node_lists_ is cleared by NodeRareData::TraceAfterDispatch().
  NodeListsNodeData& EnsureNodeLists() {
    DCHECK(ThreadState::Current()->IsGCForbidden());
    if (!node_lists_)
      return CreateNodeLists();
    return *node_lists_;
  }

  FlatTreeNodeData* GetFlatTreeNodeData() const { return flat_tree_node_data_; }
  FlatTreeNodeData& EnsureFlatTreeNodeData();

  NodeMutationObserverData* MutationObserverData() {
    return mutation_observer_data_.Get();
  }
  NodeMutationObserverData& EnsureMutationObserverData() {
    if (!mutation_observer_data_) {
      mutation_observer_data_ =
          MakeGarbageCollected<NodeMutationObserverData>();
    }
    return *mutation_observer_data_;
  }

  unsigned ConnectedSubframeCount() const { return connected_frame_count_; }
  void IncrementConnectedSubframeCount();
  void DecrementConnectedSubframeCount() {
    DCHECK(connected_frame_count_);
    --connected_frame_count_;
  }

  bool HasElementFlag(ElementFlags mask) const {
    return element_flags_ & static_cast<unsigned>(mask);
  }
  void SetElementFlag(ElementFlags mask, bool value) {
    element_flags_ = (element_flags_ & ~static_cast<unsigned>(mask)) |
                     (-(int32_t)value & static_cast<unsigned>(mask));
  }
  void ClearElementFlag(ElementFlags mask) {
    element_flags_ &= ~static_cast<unsigned>(mask);
  }

  bool HasRestyleFlag(DynamicRestyleFlags mask) const {
    return restyle_flags_ & static_cast<unsigned>(mask);
  }
  void SetRestyleFlag(DynamicRestyleFlags mask) {
    restyle_flags_ |= static_cast<unsigned>(mask);
    CHECK(restyle_flags_);
  }
  bool HasRestyleFlags() const { return restyle_flags_; }
  void ClearRestyleFlags() { restyle_flags_ = 0; }

  void TraceAfterDispatch(blink::Visitor*);
  void FinalizeGarbageCollectedObject();

 protected:
  Member<NodeRenderingData> node_layout_data_;

 private:
  NodeListsNodeData& CreateNodeLists();

  Member<NodeListsNodeData> node_lists_;
  Member<NodeMutationObserverData> mutation_observer_data_;
  Member<FlatTreeNodeData> flat_tree_node_data_;

  DISALLOW_COPY_AND_ASSIGN(NodeRareData);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NODE_RARE_DATA_H_
