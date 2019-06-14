// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_PAINT_FRAGMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_PAINT_FRAGMENT_H_

#include <iterator>
#include <memory>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_fragment.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item_client.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class NGBlockBreakToken;
struct LayoutSelectionStatus;
enum class NGOutlineType;

// The NGPaintFragment contains a NGPhysicalFragment and geometry in the paint
// coordinate system.
//
// NGPhysicalFragment is limited to its parent coordinate system for caching and
// sharing subtree. This class makes it possible to compute visual rects in the
// parent transform node.
//
// NGPaintFragment is an ImageResourceObserver, which means that it gets
// notified when associated images are changed.
// This is used for 2 main use cases:
// - reply to 'background-image' as we need to invalidate the background in this
//   case.
//   (See https://drafts.csswg.org/css-backgrounds-3/#the-background-image)
// - image (<img>, svg <image>) or video (<video>) elements that are
//   placeholders for displaying them.
class CORE_EXPORT NGPaintFragment : public RefCounted<NGPaintFragment>,
                                    public DisplayItemClient {
 public:
  NGPaintFragment(scoped_refptr<const NGPhysicalFragment>,
                  PhysicalOffset offset,
                  NGPaintFragment*);
  ~NGPaintFragment() override;

  static scoped_refptr<NGPaintFragment> Create(
      scoped_refptr<const NGPhysicalFragment>,
      const NGBlockBreakToken* break_token,
      scoped_refptr<NGPaintFragment> previous_instance = nullptr);

  const NGPhysicalFragment& PhysicalFragment() const {
    CHECK(!is_layout_object_destroyed_);
    return *physical_fragment_;
  }

  static scoped_refptr<NGPaintFragment>* Find(scoped_refptr<NGPaintFragment>*,
                                              const NGBlockBreakToken*);

  template <typename Traverse>
  class List {
    STACK_ALLOCATED();

   public:
    explicit List(NGPaintFragment* first) : first_(first) {}

    class iterator final
        : public std::iterator<std::forward_iterator_tag, NGPaintFragment*> {
     public:
      explicit iterator(NGPaintFragment* first) : current_(first) {}

      NGPaintFragment* operator*() const { return current_; }
      NGPaintFragment* operator->() const { return current_; }
      iterator& operator++() {
        DCHECK(current_);
        current_ = Traverse::Next(current_);
        return *this;
      }
      bool operator==(const iterator& other) const {
        return current_ == other.current_;
      }
      bool operator!=(const iterator& other) const {
        return current_ != other.current_;
      }

     private:
      NGPaintFragment* current_;
    };

    CORE_EXPORT iterator begin() const { return iterator(first_); }
    CORE_EXPORT iterator end() const { return iterator(nullptr); }

    // Returns the first |NGPaintFragment| in |FragmentRange| as STL container.
    // It is error to call |front()| for empty range.
    NGPaintFragment& front() const;

    // Returns the last |NGPaintFragment| in |FragmentRange| as STL container.
    // It is error to call |back()| for empty range.
    // Note: The complexity of |back()| is O(n) where n is number of elements
    // in this |FragmentRange|.
    NGPaintFragment& back() const;

    // Returns number of fragments in this range. The complexity is O(n) where n
    // is number of elements.
    wtf_size_t size() const;
    CORE_EXPORT bool IsEmpty() const { return !first_; }

    void ToList(Vector<NGPaintFragment*, 16>*) const;

   private:
    NGPaintFragment* first_;
  };

  class TraverseNextSibling {
    STATIC_ONLY(TraverseNextSibling);

   public:
    static NGPaintFragment* Next(NGPaintFragment* current) {
      return current->NextSibling();
    }
  };
  using ChildList = List<TraverseNextSibling>;

  // The parent NGPaintFragment. This is nullptr for a root; i.e., when parent
  // is not for NGPaint. In the first phase, this means that this is a root of
  // an inline formatting context.
  NGPaintFragment* Parent() const { return parent_; }
  NGPaintFragment* FirstChild() const { return FirstAlive(first_child_.get()); }
  NGPaintFragment* NextSibling() const {
    return FirstAlive(next_sibling_.get());
  }
  ChildList Children() const { return ChildList(FirstChild()); }

  // Note, as the name implies, |IsDescendantOfNotSelf| returns false for the
  // same object. This is different from |LayoutObject::IsDescendant| but is
  // same as |Node::IsDescendant|.
  bool IsDescendantOfNotSelf(const NGPaintFragment&) const;

  // Returns the first line box for a block-level container.
  NGPaintFragment* FirstLineBox() const;

  // Returns the container line box for inline fragments.
  const NGPaintFragment* ContainerLineBox() const;

  // Returns true if this fragment is line box and marked dirty.
  bool IsDirty() const { return is_dirty_inline_; }

  // Returns offset to its container box for inline and line box fragments.
  const PhysicalOffset& InlineOffsetToContainerBox() const {
    DCHECK(PhysicalFragment().IsInline() || PhysicalFragment().IsLineBox());
    return inline_offset_to_container_box_;
  }

  // InkOverflow of itself, not including contents, in the local coordinate.
  PhysicalRect SelfInkOverflow() const;

  // InkOverflow of its contents, not including itself, in the local coordinate.
  PhysicalRect ContentsInkOverflow() const;

  // InkOverflow of itself, including contents if they contribute to the ink
  // overflow of this object (e.g. when not clipped,) in the local coordinate.
  PhysicalRect InkOverflow() const;

  void RecalcInlineChildrenInkOverflow() const;

  void AddSelfOutlineRects(Vector<PhysicalRect>*,
                           const PhysicalOffset& offset,
                           NGOutlineType) const;

  // TODO(layout-dev): Implement when we have oveflow support.
  // TODO(eae): Switch to using NG geometry types.
  bool HasOverflowClip() const { return PhysicalFragment().HasOverflowClip(); }
  bool ShouldClipOverflow() const;
  bool HasSelfPaintingLayer() const;
  // This is equivalent to LayoutObject::VisualRect
  IntRect VisualRect() const override;
  IntRect PartialInvalidationVisualRect() const override;

  PhysicalRect ComputeLocalSelectionRectForText(
      const LayoutSelectionStatus&) const;
  PhysicalRect ComputeLocalSelectionRectForReplaced() const;

  // Set ShouldDoFullPaintInvalidation flag in the corresponding LayoutObject.
  void SetShouldDoFullPaintInvalidation();

  // Set ShouldDoFullPaintInvalidation flag in the corresponding LayoutObject
  // recursively.
  void SetShouldDoFullPaintInvalidationRecursively();

  // Set ShouldDoFullPaintInvalidation flag to all objects in the first line of
  // this block-level fragment.
  void SetShouldDoFullPaintInvalidationForFirstLine() const;

  // DisplayItemClient methods.
  String DebugName() const override;

  // Commonly used functions for NGPhysicalFragment.
  Node* GetNode() const { return PhysicalFragment().GetNode(); }
  const LayoutObject* GetLayoutObject() const {
    return PhysicalFragment().GetLayoutObject();
  }
  LayoutObject* GetMutableLayoutObject() const {
    return PhysicalFragment().GetMutableLayoutObject();
  }
  const ComputedStyle& Style() const { return PhysicalFragment().Style(); }
  PhysicalOffset Offset() const {
    DCHECK(parent_);
    return offset_;
  }
  PhysicalSize Size() const { return PhysicalFragment().Size(); }

  // Converts the given point, relative to the fragment itself, into a position
  // in DOM tree.
  PositionWithAffinity PositionForPoint(const PhysicalOffset&) const;

  // The node to return when hit-testing on this fragment. This can be different
  // from GetNode() when this fragment is content of a pseudo node.
  Node* NodeForHitTest() const;

  // Returns true when associated fragment of |layout_object| has line box.
  static bool TryMarkFirstLineBoxDirtyFor(const LayoutObject& layout_object);
  static bool TryMarkLastLineBoxDirtyFor(const LayoutObject& layout_object);

  // A range of fragments for |FragmentsFor()|.
  class TraverseNextForSameLayoutObject {
    STATIC_ONLY(TraverseNextForSameLayoutObject);

   public:
    static NGPaintFragment* Next(NGPaintFragment* current) {
      return current->next_for_same_layout_object_;
    }
  };
  class CORE_EXPORT FragmentRange
      : public List<TraverseNextForSameLayoutObject> {
   public:
    explicit FragmentRange(
        NGPaintFragment* first,
        bool is_in_layout_ng_inline_formatting_context = true)
        : List(first),
          is_in_layout_ng_inline_formatting_context_(
              is_in_layout_ng_inline_formatting_context) {}

    bool IsInLayoutNGInlineFormattingContext() const {
      return is_in_layout_ng_inline_formatting_context_;
    }

   private:
    bool is_in_layout_ng_inline_formatting_context_;
  };

  // Returns NGPaintFragment for the inline formatting context the LayoutObject
  // belongs to.
  //
  // When the LayoutObject is an inline block, it belongs to an inline
  // formatting context while it creates its own for its descendants. This
  // function always returns the one it belongs to.
  static const NGPaintFragment* GetForInlineContainer(const LayoutObject*);

  // Returns a range of NGPaintFragment in an inline formatting context that are
  // for a LayoutObject.
  static FragmentRange InlineFragmentsFor(const LayoutObject*);
  // A safer version that returns empty if the block flow is dirty.
  // TODO(kojii): If the block flow is dirty, children of these fragments maybe
  // already deleted. crbug.com/963103
  static FragmentRange SafeInlineFragmentsFor(const LayoutObject*);

  // Same as |InlineFragmentsFor()| but this function includes descendants if
  // the |layout_object| is culled (i.e., did not generate fragments.)
  template <typename Callback>
  static void InlineFragmentsIncludingCulledFor(const LayoutObject&,
                                                Callback callback);

  const NGPaintFragment* LastForSameLayoutObject() const;
  NGPaintFragment* LastForSameLayoutObject();
  void LayoutObjectWillBeDestroyed();

  void ClearAssociationWithLayoutObject();

  // Called when lines containing |child| is dirty.
  static void DirtyLinesFromChangedChild(LayoutObject* child);

  // Mark this line box was changed, in order to re-use part of an inline
  // formatting context.
  void MarkLineBoxDirty() {
    DCHECK(PhysicalFragment().IsLineBox());
    is_dirty_inline_ = true;
  }

  // Mark the line box that contains this fragment dirty.
  void MarkContainingLineBoxDirty();

  // Computes LocalVisualRect for an inline LayoutObject. Returns nullopt if the
  // LayoutObject is not in LayoutNG inline formatting context.
  static base::Optional<PhysicalRect> LocalVisualRectFor(const LayoutObject&);

 private:
  // Returns the first "alive" fragment; i.e., fragment that doesn't have
  // destroyed LayoutObject.
  static NGPaintFragment* FirstAlive(NGPaintFragment* fragment) {
    while (UNLIKELY(fragment && fragment->is_layout_object_destroyed_))
      fragment = fragment->next_sibling_.get();
    return fragment;
  }

  struct CreateContext {
    STACK_ALLOCATED();

   public:
    CreateContext(scoped_refptr<NGPaintFragment> previous_instance,
                  bool populate_children)
        : previous_instance(std::move(previous_instance)),
          populate_children(populate_children) {}
    CreateContext(CreateContext* parent_context, NGPaintFragment* parent)
        : parent(parent),
          last_fragment_map(parent_context->last_fragment_map),
          previous_instance(std::move(parent->first_child_)) {}

    void SkipDestroyedPreviousInstances();
    void DestroyPreviousInstances();

    NGPaintFragment* parent = nullptr;
    HashMap<const LayoutObject*, NGPaintFragment*>* last_fragment_map = nullptr;
    scoped_refptr<NGPaintFragment> previous_instance;
    bool populate_children = false;
    bool painting_layer_needs_repaint = false;
  };
  static scoped_refptr<NGPaintFragment> CreateOrReuse(
      scoped_refptr<const NGPhysicalFragment> fragment,
      PhysicalOffset offset,
      CreateContext* context);

  void PopulateDescendants(CreateContext* parent_context);
  void AssociateWithLayoutObject(
      LayoutObject*,
      HashMap<const LayoutObject*, NGPaintFragment*>* last_fragment_map);

  static void DestroyAll(scoped_refptr<NGPaintFragment> fragment);
  void RemoveChildren();

  // Helps for PositionForPoint() when |this| falls in different categories.
  PositionWithAffinity PositionForPointInText(const PhysicalOffset&) const;
  PositionWithAffinity PositionForPointInInlineFormattingContext(
      const PhysicalOffset&) const;
  PositionWithAffinity PositionForPointInInlineLevelBox(
      const PhysicalOffset&) const;

  // Dirty line boxes containing |layout_object|.
  static void MarkLineBoxesDirtyFor(const LayoutObject& layout_object);

  // Returns |LayoutBox| that holds ink overflow for this fragment.
  const LayoutBox* InkOverflowOwnerBox() const;

  // Re-compute ink overflow of children and return the union.
  PhysicalRect RecalcInkOverflow();
  PhysicalRect RecalcContentsInkOverflow() const;

  // This fragment will use the layout object's visual rect.
  const LayoutObject& VisualRectLayoutObject(bool& this_as_inline_box) const;

  //
  // Following fields are computed in the layout phase.
  //

  scoped_refptr<const NGPhysicalFragment> physical_fragment_;
  PhysicalOffset offset_;

  NGPaintFragment* parent_;
  scoped_refptr<NGPaintFragment> first_child_;
  scoped_refptr<NGPaintFragment> next_sibling_;

  // The next fragment for when this is fragmented.
  scoped_refptr<NGPaintFragment> next_fragmented_;

  NGPaintFragment* next_for_same_layout_object_ = nullptr;
  PhysicalOffset inline_offset_to_container_box_;

  // The ink overflow storage for when |InkOverflowOwnerBox()| is nullptr.
  struct NGInkOverflowModel {
    USING_FAST_MALLOC(NGInkOverflowModel);

   public:
    NGInkOverflowModel(const PhysicalRect& self_ink_overflow,
                       const PhysicalRect& contents_ink_overflow);

    PhysicalRect self_ink_overflow;
    PhysicalRect contents_ink_overflow;
  };
  std::unique_ptr<NGInkOverflowModel> ink_overflow_;

  // Set when the corresponding LayoutObject is destroyed.
  unsigned is_layout_object_destroyed_ : 1;

  // For a line box, this indicates it is dirty. This helps to determine if the
  // fragment is re-usable when part of an inline formatting context is changed.
  // For an inline box, this flag helps to avoid traversing up to its line box
  // every time.
  unsigned is_dirty_inline_ : 1;
};

template <typename Callback>
void NGPaintFragment::InlineFragmentsIncludingCulledFor(
    const LayoutObject& layout_object,
    Callback callback) {
  DCHECK(layout_object.IsInLayoutNGInlineFormattingContext());

  auto fragments = InlineFragmentsFor(&layout_object);
  if (!fragments.IsEmpty()) {
    for (const NGPaintFragment* fragment : fragments)
      callback(fragment);
    return;
  }

  // This is a culled LayoutInline. Iterate children's fragments.
  if (const LayoutInline* layout_inline =
          ToLayoutInlineOrNull(&layout_object)) {
    for (LayoutObject* child = layout_inline->FirstChild(); child;
         child = child->NextSibling()) {
      // |layout_inline| may still have non-inline children, e.g.,
      // 'position:absolute'. Skip them as they don't contribute to the culled
      // rects of |layout_inline|.
      if (!child->IsInline())
        continue;
      InlineFragmentsIncludingCulledFor(*child, callback);
    }
  }
}

extern template class CORE_EXTERN_TEMPLATE_EXPORT
    NGPaintFragment::List<NGPaintFragment::TraverseNextForSameLayoutObject>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    NGPaintFragment::List<NGPaintFragment::TraverseNextSibling>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_PAINT_FRAGMENT_H_
