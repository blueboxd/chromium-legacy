// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/openscreen/platform/chrome_platform_client.h"

#include "base/threading/thread_checker.h"
#include <utility>

namespace media_router {

ChromePlatformClient::~ChromePlatformClient() = default;

// static
void ChromePlatformClient::Create(
    std::unique_ptr<network::mojom::NetworkContext> network_context,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  PlatformClient::SetInstance(new ChromePlatformClient(
      std::move(network_context), std::move(task_runner)));
}

// static
ChromePlatformClient* ChromePlatformClient::GetInstance() {
  return reinterpret_cast<ChromePlatformClient*>(PlatformClient::GetInstance());
}

// static
void ChromePlatformClient::ShutDown() {
  ChromePlatformClient* client = ChromePlatformClient::GetInstance();
  if (client != nullptr) {
    DCHECK_CALLED_ON_VALID_THREAD(client->thread_checker_);
    PlatformClient::ShutDown();
  }
}

openscreen::platform::TaskRunner* ChromePlatformClient::GetTaskRunner() {
  return task_runner_.get();
}

ChromePlatformClient::ChromePlatformClient(
    std::unique_ptr<network::mojom::NetworkContext> network_context,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : network_context_(std::move(network_context)),
      task_runner_(new ChromeTaskRunner(std::move(task_runner))) {}

}  // namespace media_router
