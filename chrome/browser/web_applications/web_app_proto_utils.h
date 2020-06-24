// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PROTO_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PROTO_UTILS_H_

#include <vector>

#include "base/optional.h"
#include "chrome/browser/web_applications/web_app.h"
#include "components/sync/protocol/web_app_specifics.pb.h"

namespace web_app {

using RepeatedIconInfosProto =
    const ::google::protobuf::RepeatedPtrField<::sync_pb::WebAppIconInfo>;

base::Optional<std::vector<WebApplicationIconInfo>> ParseWebAppIconInfos(
    const char* container_name_for_logging,
    RepeatedIconInfosProto icon_infos_proto);

base::Optional<WebApp::SyncFallbackData> ParseSyncFallbackDataStruct(
    const sync_pb::WebAppSpecifics& sync_proto);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PROTO_UTILS_H_
