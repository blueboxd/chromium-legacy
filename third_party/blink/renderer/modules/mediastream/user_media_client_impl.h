// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_USER_MEDIA_CLIENT_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_USER_MEDIA_CLIENT_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "third_party/blink/public/common/mediastream/media_devices.h"
#include "third_party/blink/public/mojom/mediastream/media_devices.mojom-blink.h"
#include "third_party/blink/public/web/web_apply_constraints_request.h"
#include "third_party/blink/public/web/web_user_media_client.h"
#include "third_party/blink/public/web/web_user_media_request.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_processor.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {

class ApplyConstraintsProcessor;
class LocalFrame;
class WebMediaStreamDeviceObserver;

// UserMediaClientImpl handles requests coming from the Blink MediaDevices
// object. This includes getUserMedia and enumerateDevices. It must be created,
// called and destroyed on the render thread.
class MODULES_EXPORT UserMediaClientImpl : public blink::WebUserMediaClient {
 public:
  // TODO(guidou): Make all constructors private and replace with Create methods
  // that return a std::unique_ptr. This class is intended for instantiation on
  // the free store. https://crbug.com/764293
  // |frame| and its respective RenderFrame must outlive this instance.
  UserMediaClientImpl(LocalFrame* frame,
                      std::unique_ptr<blink::WebMediaStreamDeviceObserver>
                          media_stream_device_observer,
                      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  UserMediaClientImpl(LocalFrame* frame,
                      std::unique_ptr<UserMediaProcessor> user_media_processor,
                      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~UserMediaClientImpl() override;

  blink::WebMediaStreamDeviceObserver* GetMediaStreamDeviceObserver() override {
    return user_media_processor_->media_stream_device_observer();
  }

  // blink::WebUserMediaClient implementation
  void RequestUserMedia(const blink::WebUserMediaRequest& web_request) override;
  void CancelUserMediaRequest(
      const blink::WebUserMediaRequest& web_request) override;
  void ApplyConstraints(
      const blink::WebApplyConstraintsRequest& web_request) override;
  void StopTrack(const blink::WebMediaStreamTrack& web_track) override;
  bool IsCapturing() override;
  void ContextDestroyed() override;

  void SetMediaDevicesDispatcherForTesting(
      blink::mojom::blink::MediaDevicesDispatcherHostPtr
          media_devices_dispatcher);

 private:
  class Request {
   public:
    explicit Request(std::unique_ptr<UserMediaRequestInfo> request);
    explicit Request(const blink::WebApplyConstraintsRequest& request);
    explicit Request(const blink::WebMediaStreamTrack& request);
    Request(Request&& other);
    Request& operator=(Request&& other);
    ~Request();

    std::unique_ptr<UserMediaRequestInfo> MoveUserMediaRequest();

    UserMediaRequestInfo* user_media_request() const {
      return user_media_request_.get();
    }
    const blink::WebApplyConstraintsRequest& apply_constraints_request() const {
      return apply_constraints_request_;
    }
    const blink::WebMediaStreamTrack& web_track_to_stop() const {
      return web_track_to_stop_;
    }

    bool IsUserMedia() const { return !!user_media_request_; }
    bool IsApplyConstraints() const {
      return !apply_constraints_request_.IsNull();
    }
    bool IsStopTrack() const { return !web_track_to_stop_.IsNull(); }

   private:
    std::unique_ptr<UserMediaRequestInfo> user_media_request_;
    blink::WebApplyConstraintsRequest apply_constraints_request_;
    blink::WebMediaStreamTrack web_track_to_stop_;
  };

  void MaybeProcessNextRequestInfo();
  void CurrentRequestCompleted();

  void DeleteAllUserMediaRequests();

  const blink::mojom::blink::MediaDevicesDispatcherHostPtr&
  GetMediaDevicesDispatcher();

  // LocalFrame instance associated with the RenderFrameImpl that
  // own this UserMediaClientImpl.
  //
  // TODO(crbug.com/704136): Consider moving UserMediaClientImpl to
  // Oilpan and use a Member.
  WeakPersistent<LocalFrame> frame_;

  // |user_media_processor_| is a unique_ptr for testing purposes.
  std::unique_ptr<UserMediaProcessor> user_media_processor_;
  // |user_media_processor_| is a unique_ptr in order to avoid compilation
  // problems in builds that do not include WebRTC.
  std::unique_ptr<ApplyConstraintsProcessor> apply_constraints_processor_;

  blink::mojom::blink::MediaDevicesDispatcherHostPtr media_devices_dispatcher_;

  // UserMedia requests are processed sequentially. |is_processing_request_|
  // is a flag that indicates if a request is being processed at a given time,
  // and |pending_request_infos_| is a list of queued requests.
  bool is_processing_request_ = false;

  Deque<Request> pending_request_infos_;

  THREAD_CHECKER(thread_checker_);

  // Note: This member must be the last to ensure all outstanding weak pointers
  // are invalidated first.
  base::WeakPtrFactory<UserMediaClientImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(UserMediaClientImpl);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_USER_MEDIA_CLIENT_IMPL_H_
