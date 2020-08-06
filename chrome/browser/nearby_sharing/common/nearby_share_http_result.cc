// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/common/nearby_share_http_result.h"

NearbyShareHttpError NearbyShareHttpErrorForHttpResponseCode(
    int response_code) {
  if (response_code == 400)
    return NearbyShareHttpError::kBadRequest;

  if (response_code == 403)
    return NearbyShareHttpError::kAuthenticationError;

  if (response_code == 404)
    return NearbyShareHttpError::kEndpointNotFound;

  if (response_code >= 500 && response_code < 600)
    return NearbyShareHttpError::kInternalServerError;

  return NearbyShareHttpError::kUnknown;
}

NearbyShareHttpResult NearbyShareHttpErrorToResult(NearbyShareHttpError error) {
  switch (error) {
    case NearbyShareHttpError::kOffline:
      return NearbyShareHttpResult::kHttpErrorOffline;
    case NearbyShareHttpError::kEndpointNotFound:
      return NearbyShareHttpResult::kHttpErrorEndpointNotFound;
    case NearbyShareHttpError::kAuthenticationError:
      return NearbyShareHttpResult::kHttpErrorAuthenticationError;
    case NearbyShareHttpError::kBadRequest:
      return NearbyShareHttpResult::kHttpErrorBadRequest;
    case NearbyShareHttpError::kResponseMalformed:
      return NearbyShareHttpResult::kHttpErrorResponseMalformed;
    case NearbyShareHttpError::kInternalServerError:
      return NearbyShareHttpResult::kHttpErrorInternalServerError;
    case NearbyShareHttpError::kUnknown:
      return NearbyShareHttpResult::kHttpErrorUnknown;
  }
}

std::ostream& operator<<(std::ostream& stream,
                         const NearbyShareHttpResult& result) {
  switch (result) {
    case NearbyShareHttpResult::kSuccess:
      stream << "[Success]";
      break;
    case NearbyShareHttpResult::kTimeout:
      stream << "[Timeout]";
      break;
    case NearbyShareHttpResult::kHttpErrorOffline:
      stream << "[HTTP Error: Offline]";
      break;
    case NearbyShareHttpResult::kHttpErrorEndpointNotFound:
      stream << "[HTTP Error: Endpoint not found]";
      break;
    case NearbyShareHttpResult::kHttpErrorAuthenticationError:
      stream << "[HTTP Error: Authentication error]";
      break;
    case NearbyShareHttpResult::kHttpErrorBadRequest:
      stream << "[HTTP Error: Bad request]";
      break;
    case NearbyShareHttpResult::kHttpErrorResponseMalformed:
      stream << "[HTTP Error: Response malformed]";
      break;
    case NearbyShareHttpResult::kHttpErrorInternalServerError:
      stream << "[HTTP Error: Internal server error]";
      break;
    case NearbyShareHttpResult::kHttpErrorUnknown:
      stream << "[HTTP Error: Unknown]";
      break;
  }
  return stream;
}

std::ostream& operator<<(std::ostream& stream,
                         const NearbyShareHttpError& error) {
  stream << NearbyShareHttpErrorToResult(error);
  return stream;
}
