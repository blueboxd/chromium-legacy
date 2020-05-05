// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/navigator_ua.h"

namespace blink {

NavigatorUAData* NavigatorUA::userAgentData() {
  NavigatorUAData* ua_data =
      MakeGarbageCollected<NavigatorUAData>(GetUAExecutionContext());

  UserAgentMetadata metadata = GetUserAgentMetadata();
  ua_data->SetBrandVersionList(metadata.brand_version_list);
  ua_data->SetMobile(metadata.mobile);
  ua_data->SetPlatform(String::FromUTF8(metadata.platform),
                       String::FromUTF8(metadata.platform_version));
  ua_data->SetArchitecture(String::FromUTF8(metadata.architecture));
  ua_data->SetModel(String::FromUTF8(metadata.model));
  ua_data->SetUAFullVersion(String::FromUTF8(metadata.full_version));

  return ua_data;
}

}  // namespace blink
