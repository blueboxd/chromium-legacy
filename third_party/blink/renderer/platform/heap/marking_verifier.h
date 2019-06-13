// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_MARKING_VERIFIER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_MARKING_VERIFIER_H_

#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap_page.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

// Marking verifier that checks that a child is marked if its parent is marked.
class MarkingVerifier final : public Visitor {
 public:
  explicit MarkingVerifier(ThreadState* state) : Visitor(state) {}
  ~MarkingVerifier() override {}

  void VerifyObject(HeapObjectHeader* header) {
    // Verify only non-free marked objects.
    if (header->IsFree() || !header->IsMarked())
      return;

    const GCInfo* info =
        GCInfoTable::Get().GCInfoFromIndex(header->GcInfoIndex());
    const bool can_verify =
        !info->has_v_table || blink::VTableInitialized(header->Payload());
    if (can_verify) {
      CHECK(header->IsValid());
      info->trace(this, header->Payload());
    }
  }

  void Visit(void* object, TraceDescriptor desc) final {
    VerifyChild(object, desc.base_object_payload);
  }

  void VisitWeak(void* object,
                 void** object_slot,
                 TraceDescriptor desc,
                 WeakCallback callback) final {
    VerifyChild(object, desc.base_object_payload);
  }

  // Unused overrides.
  void VisitBackingStoreStrongly(const char*,
                                 void*,
                                 void**,
                                 TraceDescriptor) final {}
  void VisitBackingStoreWeakly(const char*,
                               void*,
                               void**,
                               TraceDescriptor,
                               WeakCallback,
                               void*) final {}
  void VisitBackingStoreOnly(const char*, void*, void**) final {}
  void RegisterBackingStoreCallback(void**, MovingObjectCallback, void*) final {
  }
  void RegisterWeakCallback(void*, WeakCallback) final {}
  void Visit(const TraceWrapperV8Reference<v8::Value>&) final {}

 private:
  void VerifyChild(void* object, void* base_object_payload) {
    CHECK(object);
    // Verification may check objects that are currently under construction and
    // would require vtable access to figure out their headers. A nullptr in
    // |base_object_payload| indicates that a mixin object is in construction
    // and the vtable cannot be used to get to the object header.
    const HeapObjectHeader* const child_header =
        (base_object_payload)
            ? HeapObjectHeader::FromPayload(base_object_payload)
            : HeapObjectHeader::FromInnerAddress(object);
    // These checks ensure that any children reachable from marked parents are
    // also marked. If you hit these checks then marking is in an inconsistent
    // state meaning that there are unmarked objects reachable from marked
    // ones.
    CHECK(child_header);
    CHECK(child_header->IsMarked());
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_MARKING_VERIFIER_H_
