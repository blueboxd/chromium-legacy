// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_TEST_MOCK_LOGGER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_TEST_MOCK_LOGGER_H_

#include "components/media_router/common/mojom/logger.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media_router {

class MockLogger : public mojom::Logger {
 public:
  MockLogger();
  ~MockLogger() override;

  MOCK_METHOD6(LogInfo,
               void(mojom::LogCategory category,
                    const std::string& component,
                    const std::string& message,
                    const std::string& sink_id,
                    const std::string& media_source,
                    const std::string& session_id));
  MOCK_METHOD6(LogWarning,
               void(mojom::LogCategory category,
                    const std::string& component,
                    const std::string& message,
                    const std::string& sink_id,
                    const std::string& media_source,
                    const std::string& session_id));
  MOCK_METHOD6(LogError,
               void(mojom::LogCategory category,
                    const std::string& component,
                    const std::string& message,
                    const std::string& sink_id,
                    const std::string& media_source,
                    const std::string& session_id));
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_TEST_MOCK_LOGGER_H_
