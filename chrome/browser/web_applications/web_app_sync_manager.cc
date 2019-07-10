// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_sync_manager.h"

#include "base/bind.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/channel_info.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model_impl/client_tag_based_model_type_processor.h"

namespace web_app {

WebAppSyncManager::WebAppSyncManager() {
  auto change_processor =
      std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
          syncer::WEB_APPS,
          base::BindRepeating(&syncer::ReportUnrecoverableError,
                              chrome::GetChannel()));

  bridge_ = std::make_unique<WebAppSyncBridge>(std::move(change_processor));
}

WebAppSyncManager::~WebAppSyncManager() = default;

}  // namespace web_app
