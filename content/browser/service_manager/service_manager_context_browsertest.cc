// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "content/public/browser/system_connector.h"
#include "content/public/test/content_browser_test.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/mojom/constants.mojom.h"
#include "services/service_manager/public/mojom/service_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class ServiceInstanceListener
    : public service_manager::mojom::ServiceManagerListener {
 public:
  explicit ServiceInstanceListener(
      service_manager::mojom::ServiceManagerListenerRequest request)
      : receiver_(this, std::move(request)) {}
  ~ServiceInstanceListener() override = default;

  void WaitForInit() {
    base::RunLoop loop;
    init_wait_loop_ = &loop;
    loop.Run();
    init_wait_loop_ = nullptr;
  }

  uint32_t WaitForServicePID(const std::string& service_name) {
    base::RunLoop loop;
    pid_wait_loop_ = &loop;
    service_expecting_pid_ = service_name;
    loop.Run();
    pid_wait_loop_ = nullptr;
    return pid_received_;
  }

 private:
  // service_manager::mojom::ServiceManagerListener:
  void OnInit(std::vector<service_manager::mojom::RunningServiceInfoPtr>
                  instances) override {
    if (init_wait_loop_)
      init_wait_loop_->Quit();
  }

  void OnServiceCreated(
      service_manager::mojom::RunningServiceInfoPtr instance) override {}
  void OnServiceStarted(const service_manager::Identity&,
                        uint32_t pid) override {}
  void OnServiceFailedToStart(const service_manager::Identity&) override {}
  void OnServiceStopped(const service_manager::Identity&) override {}

  void OnServicePIDReceived(const service_manager::Identity& identity,
                            uint32_t pid) override {
    if (identity.name() == service_expecting_pid_ && pid_wait_loop_) {
      pid_received_ = pid;
      pid_wait_loop_->Quit();
    }
  }

  base::RunLoop* init_wait_loop_ = nullptr;
  base::RunLoop* pid_wait_loop_ = nullptr;
  std::string service_expecting_pid_;
  uint32_t pid_received_ = 0;
  mojo::Receiver<service_manager::mojom::ServiceManagerListener> receiver_;

  DISALLOW_COPY_AND_ASSIGN(ServiceInstanceListener);
};

}  // namespace

using ServiceManagerContextBrowserTest = ContentBrowserTest;

IN_PROC_BROWSER_TEST_F(ServiceManagerContextBrowserTest,
                       ServiceProcessReportsPID) {
  service_manager::mojom::ServiceManagerListenerPtr listener_proxy;
  ServiceInstanceListener listener(mojo::MakeRequest(&listener_proxy));

  auto* connector = GetSystemConnector();
  service_manager::mojom::ServiceManagerPtr service_manager;
  connector->BindInterface(service_manager::mojom::kServiceName,
                           &service_manager);
  service_manager->AddListener(std::move(listener_proxy));
  listener.WaitForInit();

  connector->WarmService(service_manager::ServiceFilter::ByName(
      data_decoder::mojom::kServiceName));

  // PID should be non-zero, confirming that it was indeed properly reported to
  // the Service Manager. If not reported at all, this will hang.
  EXPECT_GT(listener.WaitForServicePID(data_decoder::mojom::kServiceName), 0u);
}

}  // namespace content
