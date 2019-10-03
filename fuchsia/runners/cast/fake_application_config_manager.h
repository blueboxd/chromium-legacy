// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_RUNNERS_CAST_FAKE_APPLICATION_CONFIG_MANAGER_H_
#define FUCHSIA_RUNNERS_CAST_FAKE_APPLICATION_CONFIG_MANAGER_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "fuchsia/fidl/chromium/cast/cpp/fidl.h"
#include "url/gurl.h"

// Test cast.ApplicationConfigManager implementation which maps a test Cast
// AppId to a URL.
class FakeApplicationConfigManager
    : public chromium::cast::ApplicationConfigManager {
 public:
  FakeApplicationConfigManager();
  ~FakeApplicationConfigManager() override;

  // Associates a Cast application |id| with a url, to be served from the
  // EmbeddedTestServer.
  void AddAppMapping(const std::string& id, const GURL& url);

  // chromium::cast::ApplicationConfigManager interface.
  void GetConfig(std::string id, GetConfigCallback config_callback) override;

 private:
  std::map<std::string, GURL> id_to_url_;

  DISALLOW_COPY_AND_ASSIGN(FakeApplicationConfigManager);
};

#endif  // FUCHSIA_RUNNERS_CAST_FAKE_APPLICATION_CONFIG_MANAGER_H_
