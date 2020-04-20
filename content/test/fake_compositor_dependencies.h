// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_FAKE_COMPOSITOR_DEPENDENCIES_H_
#define CONTENT_TEST_FAKE_COMPOSITOR_DEPENDENCIES_H_

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "cc/test/test_task_graph_runner.h"
#include "content/renderer/compositor/compositor_dependencies.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/platform/scheduler/test/web_fake_thread_scheduler.h"

namespace cc {
class FakeLayerTreeFrameSink;
}

namespace content {

class FakeCompositorDependencies : public CompositorDependencies {
 public:
  FakeCompositorDependencies();
  ~FakeCompositorDependencies() override;

  // CompositorDependencies implementation.
  int GetGpuRasterizationMSAASampleCount() override;
  bool IsLcdTextEnabled() override;
  bool IsZeroCopyEnabled() override;
  bool IsPartialRasterEnabled() override;
  bool IsGpuMemoryBufferCompositorResourcesEnabled() override;
  bool IsElasticOverscrollEnabled() override;
  bool IsUseZoomForDSFEnabled() override;
  bool IsSingleThreaded() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetCleanupTaskRunner() override;
  blink::scheduler::WebThreadScheduler* GetWebMainThreadScheduler() override;
  cc::TaskGraphRunner* GetTaskGraphRunner() override;
  bool IsScrollAnimatorEnabled() override;
  std::unique_ptr<cc::UkmRecorderFactory> CreateUkmRecorderFactory() override;
  void RequestNewLayerTreeFrameSink(
      RenderWidget* render_widget,
      scoped_refptr<FrameSwapMessageQueue> frame_swap_message_queue,
      const GURL& url,
      LayerTreeFrameSinkCallback callback,
      const char* client_name) override;
#ifdef OS_ANDROID
  bool UsingSynchronousCompositing() override;
#endif

  void set_use_zoom_for_dsf_enabled(bool enabled) {
    use_zoom_for_dsf_ = enabled;
  }

  // The returned pointer is valid after RequestNewLayerTreeFrameSink() occurs,
  // until another call to RequestNewLayerTreeFrameSink() happens. It's okay to
  // use this pointer on the main thread because this class causes the
  // compositor to run in single thread mode by returning a null from
  // GetCompositorImplThreadTaskRunner().
  cc::FakeLayerTreeFrameSink* last_created_frame_sink() {
    return last_created_frame_sink_;
  }

 private:
  cc::TestTaskGraphRunner task_graph_runner_;
  blink::scheduler::WebFakeThreadScheduler main_thread_scheduler_;
  bool use_zoom_for_dsf_ = false;
  cc::FakeLayerTreeFrameSink* last_created_frame_sink_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FakeCompositorDependencies);
};

}  // namespace content

#endif  // CONTENT_TEST_FAKE_COMPOSITOR_DEPENDENCIES_H_
