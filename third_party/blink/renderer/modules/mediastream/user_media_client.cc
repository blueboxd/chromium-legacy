// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/user_media_client.h"

#include <stddef.h>
#include <algorithm>
#include <utility>

#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/platform/modules/mediastream/webrtc_uma_histograms.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_media_constraints.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/public/web/modules/mediastream/web_media_stream_device_observer.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_user_gesture_indicator.h"
#include "third_party/blink/public/web/web_user_media_request.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/mediastream/apply_constraints_processor.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {
namespace {

static int g_next_request_id = 0;

// The histogram counts the number of calls to the JS API
// getUserMedia or getDisplayMedia().
void UpdateAPICount(blink::WebUserMediaRequest::MediaType media_type) {
  blink::WebRTCAPIName api_name = blink::WebRTCAPIName::kGetUserMedia;
  switch (media_type) {
    case blink::WebUserMediaRequest::MediaType::kUserMedia:
      api_name = blink::WebRTCAPIName::kGetUserMedia;
      break;
    case blink::WebUserMediaRequest::MediaType::kDisplayMedia:
      api_name = blink::WebRTCAPIName::kGetDisplayMedia;
      break;
  }
  UpdateWebRTCMethodCount(api_name);
}

}  // namespace

UserMediaClient::Request::Request(std::unique_ptr<UserMediaRequestInfo> request)
    : user_media_request_(std::move(request)) {
  DCHECK(user_media_request_);
  DCHECK(apply_constraints_request_.IsNull());
  DCHECK(web_track_to_stop_.IsNull());
}

UserMediaClient::Request::Request(
    const blink::WebApplyConstraintsRequest& request)
    : apply_constraints_request_(request) {
  DCHECK(!apply_constraints_request_.IsNull());
  DCHECK(!user_media_request_);
  DCHECK(web_track_to_stop_.IsNull());
}

UserMediaClient::Request::Request(
    const blink::WebMediaStreamTrack& web_track_to_stop)
    : web_track_to_stop_(web_track_to_stop) {
  DCHECK(!web_track_to_stop_.IsNull());
  DCHECK(!user_media_request_);
  DCHECK(apply_constraints_request_.IsNull());
}

UserMediaClient::Request::Request(Request&& other)
    : user_media_request_(std::move(other.user_media_request_)),
      apply_constraints_request_(other.apply_constraints_request_),
      web_track_to_stop_(other.web_track_to_stop_) {
#if DCHECK_IS_ON()
  int num_types = 0;
  if (IsUserMedia())
    num_types++;
  if (IsApplyConstraints())
    num_types++;
  if (IsStopTrack())
    num_types++;

  DCHECK_EQ(num_types, 1);
#endif
}

UserMediaClient::Request& UserMediaClient::Request::operator=(Request&& other) =
    default;
UserMediaClient::Request::~Request() = default;

std::unique_ptr<UserMediaRequestInfo>
UserMediaClient::Request::MoveUserMediaRequest() {
  return std::move(user_media_request_);
}

UserMediaClient::UserMediaClient(
    LocalFrame* frame,
    std::unique_ptr<UserMediaProcessor> user_media_processor,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : frame_(frame),
      user_media_processor_(std::move(user_media_processor)),
      apply_constraints_processor_(new ApplyConstraintsProcessor(
          WTF::BindRepeating(&UserMediaClient::GetMediaDevicesDispatcher,
                             WTF::Unretained(this)),
          std::move(task_runner))) {
  if (frame_) {
    // WTF::Unretained is safe because the |frame_| owns UMCI.
    frame_->SetIsCapturingMediaCallback(WTF::BindRepeating(
        &UserMediaClient::IsCapturing, WTF::Unretained(this)));
  }
}

// WTF::Unretained(this) is safe here because |this| owns
// |user_media_processor_|.
UserMediaClient::UserMediaClient(
    LocalFrame* frame,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : UserMediaClient(
          frame,
          std::make_unique<UserMediaProcessor>(
              frame,
              WTF::BindRepeating(&UserMediaClient::GetMediaDevicesDispatcher,
                                 WTF::Unretained(this)),
              frame->GetTaskRunner(blink::TaskType::kInternalMedia)),
          std::move(task_runner)) {}

UserMediaClient::~UserMediaClient() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Force-close all outstanding user media requests and local sources here,
  // before the outstanding WeakPtrs are invalidated, to ensure a clean
  // shutdown.
  DeleteAllUserMediaRequests();
}

void UserMediaClient::RequestUserMedia(
    const blink::WebUserMediaRequest& web_request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!web_request.IsNull());
  DCHECK(web_request.Audio() || web_request.Video());
  // ownerDocument may be null if we are in a test.
  // In that case, it's OK to not check frame().

  DCHECK(web_request.OwnerDocument().IsNull() ||
         WebFrame::FromFrame(frame_) ==
             static_cast<blink::WebFrame*>(
                 web_request.OwnerDocument().GetFrame()));

  // Save histogram data so we can see how much GetUserMedia is used.
  UpdateAPICount(web_request.MediaRequestType());

  // TODO(crbug.com/787254): Communicate directly with the
  // PeerConnectionTrackerHost mojo object once it is available from Blink.
  Platform::Current()->TrackGetUserMedia(web_request);

  int request_id = g_next_request_id++;
  blink::WebRtcLogMessage(base::StringPrintf(
      "UMCI::RequestUserMedia. request_id=%d, audio constraints=%s, "
      "video constraints=%s",
      request_id, web_request.AudioConstraints().ToString().Utf8().c_str(),
      web_request.VideoConstraints().ToString().Utf8().c_str()));

  // The value returned by isProcessingUserGesture() is used by the browser to
  // make decisions about the permissions UI. Its value can be lost while
  // switching threads, so saving its value here.
  bool user_gesture = blink::WebUserGestureIndicator::IsProcessingUserGesture(
      web_request.OwnerDocument().IsNull()
          ? nullptr
          : web_request.OwnerDocument().GetFrame());
  std::unique_ptr<UserMediaRequestInfo> request_info =
      std::make_unique<UserMediaRequestInfo>(request_id, web_request,
                                             user_gesture);
  pending_request_infos_.push_back(Request(std::move(request_info)));
  if (!is_processing_request_)
    MaybeProcessNextRequestInfo();
}

void UserMediaClient::ApplyConstraints(
    const blink::WebApplyConstraintsRequest& web_request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  pending_request_infos_.push_back(Request(web_request));
  if (!is_processing_request_)
    MaybeProcessNextRequestInfo();
}

void UserMediaClient::StopTrack(const blink::WebMediaStreamTrack& web_track) {
  pending_request_infos_.push_back(Request(web_track));
  if (!is_processing_request_)
    MaybeProcessNextRequestInfo();
}

bool UserMediaClient::IsCapturing() {
  return user_media_processor_->HasActiveSources();
}

void UserMediaClient::MaybeProcessNextRequestInfo() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (is_processing_request_ || pending_request_infos_.empty())
    return;

  Request current_request = std::move(pending_request_infos_.front());
  pending_request_infos_.pop_front();
  is_processing_request_ = true;

  // WTF::Unretained() is safe here because |this| owns
  // |user_media_processor_|.
  if (current_request.IsUserMedia()) {
    user_media_processor_->ProcessRequest(
        current_request.MoveUserMediaRequest(),
        WTF::Bind(&UserMediaClient::CurrentRequestCompleted,
                  WTF::Unretained(this)));
  } else if (current_request.IsApplyConstraints()) {
    apply_constraints_processor_->ProcessRequest(
        current_request.apply_constraints_request(),
        WTF::Bind(&UserMediaClient::CurrentRequestCompleted,
                  WTF::Unretained(this)));
  } else {
    DCHECK(current_request.IsStopTrack());
    blink::WebPlatformMediaStreamTrack* track =
        blink::WebPlatformMediaStreamTrack::GetTrack(
            current_request.web_track_to_stop());
    if (track) {
      track->StopAndNotify(WTF::Bind(&UserMediaClient::CurrentRequestCompleted,
                                     weak_factory_.GetWeakPtr()));
    } else {
      CurrentRequestCompleted();
    }
  }
}

void UserMediaClient::CurrentRequestCompleted() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  is_processing_request_ = false;
  if (!pending_request_infos_.empty()) {
    frame_->GetTaskRunner(blink::TaskType::kInternalMedia)
        ->PostTask(FROM_HERE,
                   WTF::Bind(&UserMediaClient::MaybeProcessNextRequestInfo,
                             weak_factory_.GetWeakPtr()));
  }
}

void UserMediaClient::CancelUserMediaRequest(
    const blink::WebUserMediaRequest& web_request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  {
    // TODO(guidou): Remove this conditional logging. https://crbug.com/764293
    UserMediaRequestInfo* request = user_media_processor_->CurrentRequest();
    if (request && request->web_request == web_request) {
      blink::WebRtcLogMessage(base::StringPrintf(
          "UMCI::CancelUserMediaRequest. request_id=%d", request->request_id));
    }
  }

  bool did_remove_request = false;
  if (user_media_processor_->DeleteWebRequest(web_request)) {
    did_remove_request = true;
  } else {
    for (auto it = pending_request_infos_.begin();
         it != pending_request_infos_.end(); ++it) {
      if (it->IsUserMedia() &&
          it->user_media_request()->web_request == web_request) {
        pending_request_infos_.erase(it);
        did_remove_request = true;
        break;
      }
    }
  }

  if (did_remove_request) {
    // We can't abort the stream generation process.
    // Instead, erase the request. Once the stream is generated we will stop the
    // stream if the request does not exist.
    LogUserMediaRequestWithNoResult(
        blink::MEDIA_STREAM_REQUEST_EXPLICITLY_CANCELLED);
  }
}

void UserMediaClient::DeleteAllUserMediaRequests() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (frame_)
    frame_->SetIsCapturingMediaCallback(LocalFrame::IsCapturingMediaCallback());
  user_media_processor_->StopAllProcessing();
  is_processing_request_ = false;
  pending_request_infos_.clear();
}

void UserMediaClient::ContextDestroyed() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Cancel all outstanding UserMediaRequests.
  DeleteAllUserMediaRequests();
}

void UserMediaClient::SetMediaDevicesDispatcherForTesting(
    blink::mojom::blink::MediaDevicesDispatcherHostPtr
        media_devices_dispatcher) {
  media_devices_dispatcher_ = std::move(media_devices_dispatcher);
}

const blink::mojom::blink::MediaDevicesDispatcherHostPtr&
UserMediaClient::GetMediaDevicesDispatcher() {
  if (!media_devices_dispatcher_) {
    frame_->GetInterfaceProvider().GetInterface(
        mojo::MakeRequest(&media_devices_dispatcher_));
  }

  return media_devices_dispatcher_;
}

}  // namespace blink
