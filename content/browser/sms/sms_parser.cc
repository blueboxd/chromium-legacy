// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "content/browser/sms/sms_parser.h"

#include "base/optional.h"
#include "net/base/url_util.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

// SMS one-time-passcode format:
// https://github.com/WebKit/explainers/blob/master/sms-one-time-code-format/README.md
constexpr char kOtpFormatRegex[] = "(?:^|\\s)@([a-zA-Z0-9.-]+) #(.[^#\\s]+)";

SmsParser::Result::Result(const url::Origin& origin,
                          const std::string& one_time_code)
    : origin(std::move(origin)), one_time_code(one_time_code) {}

SmsParser::Result::~Result() {}

// static
base::Optional<SmsParser::Result> SmsParser::Parse(base::StringPiece sms) {
  std::string url, otp;
  if (!re2::RE2::PartialMatch(sms.as_string(), kOtpFormatRegex, &url, &otp))
    return base::nullopt;

  std::string host, scheme;
  int port;
  if (!net::ParseHostAndPort(url, &host, &port))
    return base::nullopt;

  // Expect localhost to always be http.
  if (net::HostStringIsLocalhost(base::StringPiece(host))) {
    scheme = "http://";
  } else {
    scheme = "https://";
  }

  GURL gurl(scheme + url);

  if (!gurl.is_valid())
    return base::nullopt;

  return Result(url::Origin::Create(gurl), otp);
}

}  // namespace content
