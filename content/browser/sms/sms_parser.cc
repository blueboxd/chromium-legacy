// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>
#include <memory>
#include <string>

#include "content/browser/sms/sms_parser.h"

#include "base/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

constexpr base::StringPiece kToken = "For: ";

// static
base::Optional<url::Origin> SmsParser::Parse(base::StringPiece sms) {
  size_t found = sms.rfind(kToken);

  if (found == base::StringPiece::npos) {
    return base::nullopt;
  }

  base::StringPiece url = sms.substr(found + kToken.length());

  GURL gurl(url);

  if (!gurl.is_valid() || !gurl.SchemeIs(url::kHttpsScheme)) {
    return base::nullopt;
  }

  return url::Origin::Create(gurl);
}

}  // namespace content
