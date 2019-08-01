#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_TIMING_TEST_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_TIMING_TEST_HELPER_H_

#include "third_party/blink/renderer/core/paint/paint_timing_detector.h"
namespace blink {

using CallbackQueue = std::queue<WebWidgetClient::ReportTimeCallback>;
// |MockPaintTimingCallbackManager| is used to mock
// |ChromeClient::NotifySwapTime()|'s swap-time queueing and invoking for
// unit-tests. Find more details in |PaintTimingCallbackManager|.
class MockPaintTimingCallbackManager final
    : public GarbageCollectedFinalized<MockPaintTimingCallbackManager>,
      public PaintTimingCallbackManager {
  USING_GARBAGE_COLLECTED_MIXIN(MockPaintTimingCallbackManager);

 public:
  ~MockPaintTimingCallbackManager() {}
  void RegisterCallback(LocalFrame& frame,
                        ReportTimeCallback callback) override {
    callback_queue_.push(ConvertToBaseOnceCallback(std::move(callback)));
  }
  void InvokeSwapTimeCallback(base::TimeTicks swap_time) {
    DCHECK_GT(callback_queue_.size(), 0UL);
    std::move(callback_queue_.front())
        .Run(WebWidgetClient::SwapResult::kDidSwap, swap_time);
    callback_queue_.pop();
  }

  size_t CountCallbacks() { return callback_queue_.size(); }

  void Trace(Visitor* visitor) override {}

 private:
  CallbackQueue callback_queue_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_TIMING_TEST_HELPER_H_
