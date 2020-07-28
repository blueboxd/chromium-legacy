// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <memory>

#include "base/run_loop.h"
#include "chrome/services/machine_learning/public/cpp/service_connection.h"
#include "chrome/services/machine_learning/public/cpp/test_support/machine_learning_test_utils.h"
#include "chrome/services/machine_learning/public/mojom/decision_tree.mojom.h"
#include "chrome/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/service_process_info.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace machine_learning {
namespace {

class ServiceProcessObserver : public content::ServiceProcessHost::Observer {
 public:
  ServiceProcessObserver() { content::ServiceProcessHost::AddObserver(this); }
  ~ServiceProcessObserver() override {
    content::ServiceProcessHost::RemoveObserver(this);
  }

  ServiceProcessObserver(const ServiceProcessObserver&) = delete;
  ServiceProcessObserver& operator=(const ServiceProcessObserver&) = delete;

  // Whether the service is launched.
  int IsLaunched() const { return is_launched_; }

  // Whether the service is terminated normally.
  bool IsTerminated() const { return is_terminated_; }

  // Launch |launch_wait_loop_| to wait until a service launch is detected.
  void WaitForLaunch() { launch_wait_loop_.Run(); }

  // Launch |terminate_wait_loop_| to wait until a normal service termination is
  // detected.
  void WaitForTerminate() { terminate_wait_loop_.Run(); }

  void OnServiceProcessLaunched(
      const content::ServiceProcessInfo& info) override {
    if (info.IsService<mojom::MachineLearningService>()) {
      is_launched_ = true;
      if (launch_wait_loop_.running())
        launch_wait_loop_.Quit();
    }
  }

  void OnServiceProcessTerminatedNormally(
      const content::ServiceProcessInfo& info) override {
    if (info.IsService<mojom::MachineLearningService>()) {
      is_terminated_ = true;
      if (terminate_wait_loop_.running())
        terminate_wait_loop_.Quit();
    }
  }

 private:
  base::RunLoop launch_wait_loop_;
  base::RunLoop terminate_wait_loop_;
  bool is_launched_ = false;
  bool is_terminated_ = false;
};
}  // namespace

using MachineLearningServiceBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(MachineLearningServiceBrowserTest, LaunchAndTerminate) {
  ServiceProcessObserver observer;
  auto* service_connection = ServiceConnection::GetInstance();

  service_connection->GetService();
  observer.WaitForLaunch();
  EXPECT_TRUE(service_connection);
  EXPECT_TRUE(observer.IsLaunched());

  service_connection->ResetServiceForTesting();
  observer.WaitForTerminate();
  EXPECT_TRUE(observer.IsTerminated());
}

IN_PROC_BROWSER_TEST_F(MachineLearningServiceBrowserTest,
                       MultipleLaunchesReusesSharedProcess) {
  ServiceProcessObserver observer;
  auto* service_connection = ServiceConnection::GetInstance();

  auto* service_ptr1 = service_connection->GetService();
  observer.WaitForLaunch();
  EXPECT_TRUE(service_connection);
  EXPECT_TRUE(observer.IsLaunched());

  auto* service_ptr2 = service_connection->GetService();
  EXPECT_EQ(service_ptr1, service_ptr2);
}

IN_PROC_BROWSER_TEST_F(MachineLearningServiceBrowserTest,
                       LoadInvalidDecisionTreeModel) {
  ServiceProcessObserver observer;
  auto run_loop = std::make_unique<base::RunLoop>();
  auto* service_connection = ServiceConnection::GetInstance();

  mojo::Remote<mojom::DecisionTreePredictor> predictor;
  mojom::LoadModelResult result = mojom::LoadModelResult::kLoadModelError;
  service_connection->LoadDecisionTreeModel(
      mojom::DecisionTreeModelSpec::New("Invalid model spec"),
      predictor.BindNewPipeAndPassReceiver(),
      base::BindOnce(
          [](mojom::LoadModelResult* p_result, base::RunLoop* run_loop,
             mojom::LoadModelResult result) {
            *p_result = result;
            run_loop->Quit();
          },
          &result, run_loop.get()));
  run_loop->Run();
  EXPECT_TRUE(observer.IsLaunched());
  EXPECT_EQ(mojom::LoadModelResult::kModelSpecError, result);

  // Flush so that |predictor| becomes aware of potential disconnection.
  predictor.FlushForTesting();
  EXPECT_FALSE(predictor.is_connected());
}

IN_PROC_BROWSER_TEST_F(MachineLearningServiceBrowserTest,
                       LoadValidDecisionTreeModel) {
  ServiceProcessObserver observer;
  auto run_loop = std::make_unique<base::RunLoop>();
  auto* service_connection = ServiceConnection::GetInstance();

  auto model_proto = testing::GetModelProtoForPredictionResult(
      mojom::DecisionTreePredictionResult::kTrue);
  mojo::Remote<mojom::DecisionTreePredictor> predictor;
  mojom::LoadModelResult result = mojom::LoadModelResult::kLoadModelError;
  service_connection->LoadDecisionTreeModel(
      mojom::DecisionTreeModelSpec::New(model_proto->SerializeAsString()),
      predictor.BindNewPipeAndPassReceiver(),
      base::BindOnce(
          [](mojom::LoadModelResult* p_result, base::RunLoop* run_loop,
             mojom::LoadModelResult result) {
            *p_result = result;
            run_loop->Quit();
          },
          &result, run_loop.get()));
  run_loop->Run();
  EXPECT_TRUE(observer.IsLaunched());
  EXPECT_EQ(mojom::LoadModelResult::kOk, result);

  // Flush so that |predictor| becomes aware of potential disconnection.
  predictor.FlushForTesting();
  EXPECT_TRUE(predictor.is_connected());
}

}  // namespace machine_learning
