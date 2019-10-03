// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/cast/fake_application_config_manager.h"

#include <string>
#include <utility>

#include "base/logging.h"

FakeApplicationConfigManager::FakeApplicationConfigManager() = default;

FakeApplicationConfigManager::~FakeApplicationConfigManager() = default;

void FakeApplicationConfigManager::GetConfig(std::string id,
                                             GetConfigCallback callback) {
  if (id_to_url_.find(id) == id_to_url_.end()) {
    LOG(ERROR) << "Unknown Cast App ID: " << id;
    callback(chromium::cast::ApplicationConfig());
    return;
  }

  chromium::cast::ApplicationConfig app_config;
  app_config.set_id(id);
  app_config.set_display_name("Dummy test app");
  app_config.set_web_url(id_to_url_[id].spec());

  callback(std::move(app_config));
}

void FakeApplicationConfigManager::AddAppMapping(const std::string& id,
                                                 const GURL& url) {
  id_to_url_[id] = url;
}
