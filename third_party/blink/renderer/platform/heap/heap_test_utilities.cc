/*
 * Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"

#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

// static
void TestSupportingGC::PreciselyCollectGarbage(
    BlinkGC::SweepingType sweeping_type) {
  ThreadState::Current()->CollectGarbage(
      BlinkGC::kNoHeapPointersOnStack, BlinkGC::kAtomicMarking, sweeping_type,
      BlinkGC::GCReason::kForcedGCForTesting);
}

// static
void TestSupportingGC::ConservativelyCollectGarbage(
    BlinkGC::SweepingType sweeping_type) {
  ThreadState::Current()->CollectGarbage(
      BlinkGC::kHeapPointersOnStack, BlinkGC::kAtomicMarking, sweeping_type,
      BlinkGC::GCReason::kForcedGCForTesting);
}

void TestSupportingGC::ClearOutOldGarbage() {
  PreciselyCollectGarbage();
  ThreadHeap& heap = ThreadState::Current()->Heap();
  while (true) {
    size_t used = heap.ObjectPayloadSizeForTesting();
    PreciselyCollectGarbage();
    if (heap.ObjectPayloadSizeForTesting() >= used)
      break;
  }
}

void TestSupportingGC::CompleteSweepingIfNeeded() {
  if (ThreadState::Current()->IsSweepingInProgress())
    ThreadState::Current()->CompleteSweep();
}

}  // namespace blink
