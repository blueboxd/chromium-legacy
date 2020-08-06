// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_COMMON_NEARBY_SHARE_HTTP_RESULT_H_
#define CHROME_BROWSER_NEARBY_SHARING_COMMON_NEARBY_SHARE_HTTP_RESULT_H_

#include <ostream>

enum class NearbyShareHttpError {
  // Request could not be completed because the device is offline or has issues
  // sending the HTTP request.
  kOffline,

  // Server endpoint could not be found.
  kEndpointNotFound,

  // Authentication error contacting back-end.
  kAuthenticationError,

  // Request was invalid.
  kBadRequest,

  // The server responded, but the response was not formatted correctly.
  kResponseMalformed,

  // Internal server error.
  kInternalServerError,

  // Unknown result.
  kUnknown
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class NearbyShareHttpResult {
  kSuccess = 0,
  kTimeout = 1,
  kHttpErrorOffline = 2,
  kHttpErrorEndpointNotFound = 3,
  kHttpErrorAuthenticationError = 4,
  kHttpErrorBadRequest = 5,
  kHttpErrorResponseMalformed = 6,
  kHttpErrorInternalServerError = 7,
  kHttpErrorUnknown = 8,
  kMaxValue = kHttpErrorUnknown
};

NearbyShareHttpError NearbyShareHttpErrorForHttpResponseCode(int response_code);
NearbyShareHttpResult NearbyShareHttpErrorToResult(NearbyShareHttpError error);

std::ostream& operator<<(std::ostream& stream,
                         const NearbyShareHttpResult& error);
std::ostream& operator<<(std::ostream& stream,
                         const NearbyShareHttpError& error);

#endif  // CHROME_BROWSER_NEARBY_SHARING_COMMON_NEARBY_SHARE_HTTP_RESULT_H_
