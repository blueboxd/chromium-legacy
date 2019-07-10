// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_LOG_NET_LOG_CAPTURE_MODE_H_
#define NET_LOG_NET_LOG_CAPTURE_MODE_H_

#include <stdint.h>

#include "net/base/net_export.h"

namespace net {

// NetLogCaptureMode specifies the logging level.
//
// It is used to control which events are emitted to the log, and what level of
// detail is included in their parameters.
//
// The capture mode is expressed as a number, where higher values imply more
// information. The exact numeric values should not be depended on.
enum class NetLogCaptureMode : uint32_t {
  // Default logging level, which is expected to be light-weight and
  // does best-effort stripping of privacy/security sensitive data.
  //
  //  * Includes most HTTP request/response headers, but strips cookies and
  //    auth.
  //  * Does not include the full bytes read/written to sockets.
  kDefault,

  // Logging level that includes everything from kDefault, plus sensitive data
  // that it may have strippped.
  //
  //  * Includes cookies and authentication headers.
  //  * Does not include the full bytes read/written to sockets.
  kIncludeSensitive,

  // Logging level that includes everything that is possible to be logged.
  //
  //  * Includes the actual bytes read/written to sockets
  //  * Will result in large log files.
  kEverything,
};

// Returns true if |capture_mode| permits logging sensitive values such as
// cookies and credentials.
NET_EXPORT bool NetLogCaptureIncludesSensitive(NetLogCaptureMode capture_mode);

// Returns true if |capture_mode| permits logging the full request/response
// bytes from sockets.
NET_EXPORT bool NetLogCaptureIncludesSocketBytes(
    NetLogCaptureMode capture_mode);

}  // namespace net

#endif  // NET_LOG_NET_LOG_CAPTURE_MODE_H_
