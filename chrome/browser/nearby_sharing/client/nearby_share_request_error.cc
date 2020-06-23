// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/client/nearby_share_request_error.h"

std::ostream& operator<<(std::ostream& stream,
                         const NearbyShareRequestError& error) {
  switch (error) {
    case NearbyShareRequestError::kOffline:
      stream << "[offline]";
      break;
    case NearbyShareRequestError::kEndpointNotFound:
      stream << "[endpoint not found]";
      break;
    case NearbyShareRequestError::kAuthenticationError:
      stream << "[authentication error]";
      break;
    case NearbyShareRequestError::kBadRequest:
      stream << "[bad request]";
      break;
    case NearbyShareRequestError::kResponseMalformed:
      stream << "[response malformed]";
      break;
    case NearbyShareRequestError::kInternalServerError:
      stream << "[internal server error]";
      break;
    case NearbyShareRequestError::kUnknown:
      stream << "[unknown]";
      break;
  }
  return stream;
}
