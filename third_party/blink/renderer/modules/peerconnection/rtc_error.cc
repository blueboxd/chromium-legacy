// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_error.h"

#include <utility>

namespace blink {

// static
RTCError* RTCError::Create(const RTCErrorInit* init, String message) {
  return MakeGarbageCollected<RTCError>(init, std::move(message));
}

RTCError::RTCError(const RTCErrorInit* init, String message)
    : DOMException(DOMExceptionCode::kOperationError, std::move(message)),
      error_detail_(init->errorDetail()),
      sdp_line_number_(init->hasSdpLineNumber()
                           ? base::Optional<int32_t>(init->sdpLineNumber())
                           : base::nullopt),
      http_request_status_code_(
          init->hasHttpRequestStatusCode()
              ? base::Optional<int32_t>(init->httpRequestStatusCode())
              : base::nullopt),
      sctp_cause_code_(init->hasSctpCauseCode()
                           ? base::Optional<int32_t>(init->sctpCauseCode())
                           : base::nullopt),
      received_alert_(init->hasReceivedAlert()
                          ? base::Optional<uint32_t>(init->receivedAlert())
                          : base::nullopt),
      sent_alert_(init->hasSentAlert()
                      ? base::Optional<uint32_t>(init->sentAlert())
                      : base::nullopt) {}

RTCError::RTCError(webrtc::RTCError err)
    : DOMException(DOMExceptionCode::kOperationError, err.message()),
      error_detail_(webrtc::ToString(err.error_detail())),
      sctp_cause_code_(err.sctp_cause_code()
                           ? base::Optional<int32_t>(*err.sctp_cause_code())
                           : base::nullopt) {}

const String& RTCError::errorDetail() const {
  return error_detail_;
}

}  // namespace blink
