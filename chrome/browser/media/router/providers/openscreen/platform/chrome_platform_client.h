// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_OPENSCREEN_PLATFORM_CHROME_PLATFORM_CLIENT_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_OPENSCREEN_PLATFORM_CHROME_PLATFORM_CLIENT_H_

#include <memory>

#include "base/threading/thread_checker.h"
#include "chrome/browser/media/router/providers/openscreen/platform/chrome_task_runner.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/openscreen/src/platform/api/platform_client.h"

namespace media_router {

class ChromePlatformClient : public openscreen::platform::PlatformClient {
 public:
  // NOTE: The initialization and shutdown routines are not threadsafe.
  static void Create(
      std::unique_ptr<network::mojom::NetworkContext> network_context,
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  static ChromePlatformClient* GetInstance();
  static void ShutDown();

  // These methods are threadsafe.
  network::mojom::NetworkContext* network_context() const {
    return network_context_.get();
  }

  // PlatformClient overrides.
  openscreen::platform::TaskRunner* GetTaskRunner() override;

 protected:
  ~ChromePlatformClient() override;

 private:
  ChromePlatformClient(
      std::unique_ptr<network::mojom::NetworkContext> network_context,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  const std::unique_ptr<network::mojom::NetworkContext> network_context_;
  const std::unique_ptr<ChromeTaskRunner> task_runner_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_OPENSCREEN_PLATFORM_CHROME_PLATFORM_CLIENT_H_
