/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_URL_ERROR_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_URL_ERROR_H_

#include "base/logging.h"
#include "base/optional.h"
#include "net/dns/public/resolve_error_info.h"
#include "services/network/public/cpp/blocked_by_response_reason.h"
#include "services/network/public/cpp/cors/cors_error_status.h"
#include "third_party/blink/public/platform/web_url.h"

namespace blink {

// TODO(yhirano): Change this to a class.
struct WebURLError {
 public:
  enum class HasCopyInCache {
    kFalse,
    kTrue,
  };
  enum class IsWebSecurityViolation {
    kFalse,
    kTrue,
  };

  WebURLError() = delete;
  // |reason| must not be 0.
  BLINK_PLATFORM_EXPORT WebURLError(int reason, const WebURL&);
  // |reason| must not be 0.
  BLINK_PLATFORM_EXPORT WebURLError(int reason,
                                    int extended_reason,
                                    net::ResolveErrorInfo resolve_error_info,
                                    HasCopyInCache,
                                    IsWebSecurityViolation,
                                    const WebURL&);
  BLINK_PLATFORM_EXPORT WebURLError(
      network::BlockedByResponseReason blocked_reason,
      net::ResolveErrorInfo resolve_error_info,
      HasCopyInCache,
      const WebURL&);
  BLINK_PLATFORM_EXPORT WebURLError(const network::CorsErrorStatus&,
                                    HasCopyInCache,
                                    const WebURL&);

  int reason() const { return reason_; }
  int extended_reason() const { return extended_reason_; }
  const net::ResolveErrorInfo& resolve_error_info() const {
    return resolve_error_info_;
  }
  bool has_copy_in_cache() const { return has_copy_in_cache_; }
  bool is_web_security_violation() const { return is_web_security_violation_; }
  const WebURL& url() const { return url_; }
  const base::Optional<network::CorsErrorStatus> cors_error_status() const {
    return cors_error_status_;
  }
  const base::Optional<network::BlockedByResponseReason>
  blocked_by_response_reason() const {
    return blocked_by_response_reason_;
  }

 private:
  // A numeric error code detailing the reason for this error. The value must
  // not be 0.
  int reason_;

  // Additional information based on the reason_.
  int extended_reason_ = 0;

  // Detailed host resolution error information.
  net::ResolveErrorInfo resolve_error_info_;

  // A flag showing whether or not we have a (possibly stale) copy of the
  // requested resource in the cache.
  bool has_copy_in_cache_ = false;

  // True if this error is created for a web security violation.
  bool is_web_security_violation_ = false;

  // The url that failed to load.
  WebURL url_;

  // Optional CORS error details.
  base::Optional<network::CorsErrorStatus> cors_error_status_;

  // More detailed reason for failing the response with
  // ERR_net::ERR_BLOCKED_BY_RESPONSE |error_code|.
  base::Optional<network::BlockedByResponseReason> blocked_by_response_reason_;
};

}  // namespace blink

#endif
