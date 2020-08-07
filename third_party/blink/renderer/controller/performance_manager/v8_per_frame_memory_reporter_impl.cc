// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/performance_manager/v8_per_frame_memory_reporter_impl.h"

#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/platform/web_isolated_world_info.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "v8/include/v8.h"

namespace WTF {
template <>
struct HashTraits<::blink::mojom::blink::PerFrameV8MemoryUsageDataPtr>
    : GenericHashTraits<::blink::mojom::blink::PerFrameV8MemoryUsageDataPtr> {
  // The default PeekOutType is a value type that requires a copy constructor
  // in the at() getter. Override it to a reference.
  typedef const ::blink::mojom::blink::PerFrameV8MemoryUsageDataPtr&
      PeekOutType;
};

}  // namespace WTF

namespace blink {

namespace {

class FrameAssociatedMeasurementDelegate : public v8::MeasureMemoryDelegate {
 public:
  using GetPerFrameV8MemoryUsageDataCallback = mojom::blink::
      V8PerFrameMemoryReporter::GetPerFrameV8MemoryUsageDataCallback;

  explicit FrameAssociatedMeasurementDelegate(
      GetPerFrameV8MemoryUsageDataCallback&& callback)
      : callback_(std::move(callback)) {}

  ~FrameAssociatedMeasurementDelegate() override {
    if (callback_) {
      std::move(callback_).Run(
          mojom::blink::PerProcessV8MemoryUsageData::New());
    }
  }

 private:
  bool ShouldMeasure(v8::Local<v8::Context> context) override {
    // Measure all contexts.
    return true;
  }

  void MeasurementComplete(
      const std::vector<std::pair<v8::Local<v8::Context>, size_t>>&
          context_sizes_in_bytes,
      size_t unattributed_size_in_bytes) override {
    mojom::blink::PerProcessV8MemoryUsageDataPtr result =
        mojom::blink::PerProcessV8MemoryUsageData::New();

    result->unassociated_bytes_used = unattributed_size_in_bytes;

    // Keep track of the per-frame data throughout this loop.
    WTF::HashMap<blink::WebLocalFrame*,
                 mojom::blink::PerFrameV8MemoryUsageDataPtr>
        frames;
    for (const auto& context_and_size : context_sizes_in_bytes) {
      const v8::Local<v8::Context>& context = context_and_size.first;
      const size_t size = context_and_size.second;

      blink::WebLocalFrame* frame =
          blink::WebLocalFrame::FrameForContext(context);

      if (!frame) {
        // TODO(crbug.com/1080672): It would be prefereable to count the
        // V8SchemaRegistry context's overhead with unassociated_bytes, but at
        // present there isn't a public API that allows this distinction.
        ++(result->num_unassociated_contexts);
        result->unassociated_context_bytes_used += size;
      } else {
        auto it = frames.find(frame);
        mojom::blink::PerFrameV8MemoryUsageData* per_frame_resources;
        if (it != frames.end()) {
          per_frame_resources = it->value.get();
        } else {
#if DCHECK_IS_ON()
          // Check that the frame token didn't already occur.
          for (const auto& entry : frames) {
            // This frame was already added to the map by frames[frame] above.
            if (frame == entry.key)
              continue;
            DCHECK_NE(entry.key->GetFrameToken(), frame->GetFrameToken());
          }
#endif
          auto new_resources = mojom::blink::PerFrameV8MemoryUsageData::New();
          new_resources->frame_token = frame->GetFrameToken();
          per_frame_resources = new_resources.get();
          frames.Set(frame, std::move(new_resources));
        }

        mojom::blink::V8IsolatedWorldMemoryUsagePtr isolated_world_usage =
            mojom::blink::V8IsolatedWorldMemoryUsage::New();
        isolated_world_usage->bytes_used = size;
        isolated_world_usage->world_id =
            frame->GetScriptContextWorldId(context);
        static_assert(
            DOMWrapperWorld::WorldId::kMainWorldId ==
                mojom::blink::V8IsolatedWorldMemoryUsage::kMainWorldId,
            "The main world IDs must match.");

        if (isolated_world_usage->world_id !=
            DOMWrapperWorld::WorldId::kMainWorldId) {
          isolated_world_usage->stable_id =
              blink::GetIsolatedWorldStableId(context);
          isolated_world_usage->human_readable_name =
              blink::GetIsolatedWorldHumanReadableName(context);
        }
#if DCHECK_IS_ON()
        // Check that the world id didn't already occur.
        for (const auto& entry : per_frame_resources->associated_bytes) {
          DCHECK_NE(entry->world_id, isolated_world_usage->world_id);
        }
#endif

        per_frame_resources->associated_bytes.push_back(
            std::move(isolated_world_usage));
      }
    }
    // Move the per-frame memory values to the result.
    for (auto& entry : frames)
      result->associated_memory.push_back(std::move(entry.value));

    std::move(callback_).Run(std::move(result));
  }

  GetPerFrameV8MemoryUsageDataCallback callback_;
};

v8::MeasureMemoryExecution ToV8MeasureMemoryExecution(
    V8PerFrameMemoryReporterImpl::Mode mode) {
  switch (mode) {
    case V8PerFrameMemoryReporterImpl::Mode::DEFAULT:
      return v8::MeasureMemoryExecution::kDefault;
    case V8PerFrameMemoryReporterImpl::Mode::EAGER:
      return v8::MeasureMemoryExecution::kEager;
    case V8PerFrameMemoryReporterImpl::Mode::LAZY:
      return v8::MeasureMemoryExecution::kLazy;
  }
  NOTREACHED();
}

}  // namespace

// static
void V8PerFrameMemoryReporterImpl::Create(
    mojo::PendingReceiver<mojom::blink::V8PerFrameMemoryReporter> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<V8PerFrameMemoryReporterImpl>(),
                              std::move(receiver));
}

void V8PerFrameMemoryReporterImpl::GetPerFrameV8MemoryUsageData(
    V8PerFrameMemoryReporterImpl::Mode mode,
    GetPerFrameV8MemoryUsageDataCallback callback) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  if (!isolate) {
    std::move(callback).Run(mojom::blink::PerProcessV8MemoryUsageData::New());
  } else {
    std::unique_ptr<FrameAssociatedMeasurementDelegate> delegate =
        std::make_unique<FrameAssociatedMeasurementDelegate>(
            std::move(callback));

    isolate->MeasureMemory(std::move(delegate),
                           ToV8MeasureMemoryExecution(mode));
  }
}

}  // namespace blink
