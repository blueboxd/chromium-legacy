// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/local_frame.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/bindings/microtask.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class TestLocalFrameBackForwardCacheClient : public EmptyLocalFrameClient {
 public:
  TestLocalFrameBackForwardCacheClient() {}
  ~TestLocalFrameBackForwardCacheClient() override = default;

  void EvictFromBackForwardCache() override { quit_closure_.Run(); }

  void WaitUntilEvictedFromBackForwardCache() {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  base::RepeatingClosure quit_closure_;
};

class LocalFrameBackForwardCacheTest : public testing::Test,
                                       private ScopedBackForwardCacheForTest {
 public:
  LocalFrameBackForwardCacheTest() : ScopedBackForwardCacheForTest(true) {}
};

// Tests a frame in the back-forward cache (a.k.a. bfcache) is evicted on
// JavaScript execution at a microtask. Eviction is necessary to ensure that the
// frame state is immutable when the frame is in the bfcache.
// (https://www.chromestatus.com/feature/5815270035685376).
TEST_F(LocalFrameBackForwardCacheTest, EvictionOnV8ExecutionAtMicrotask) {
  auto* frame_client =
      MakeGarbageCollected<TestLocalFrameBackForwardCacheClient>();
  auto page_holder = std::make_unique<DummyPageHolder>(
      IntSize(800, 600), nullptr, frame_client,
      base::BindOnce(
          [](Settings& settings) { settings.SetScriptEnabled(true); }));

  LocalFrame* frame = &page_holder->GetFrame();

  // Freeze the frame.
  frame->SetLifecycleState(mojom::FrameLifecycleState::kFrozen);

  auto* script_state = ToScriptStateForMainWorld(frame);
  ScriptState::Scope scope(script_state);

  // There are two types of microtasks:
  //   1) V8 function
  //   2) C++ closure
  // The case 1) should never happen when the frame is in bfcache. On the other
  // hand, the case 2) can happen. See https://crbug.com/994169
  Microtask::EnqueueMicrotask(base::BindOnce(
      [](LocalFrame* frame) {
        frame->GetScriptController().ExecuteScriptInMainWorld(
            "console.log('hi');");
      },
      frame));

  frame_client->WaitUntilEvictedFromBackForwardCache();
}

}  // namespace blink
