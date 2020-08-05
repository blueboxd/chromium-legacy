// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_MACHINE_LEARNING_PUBLIC_CPP_TEST_SUPPORT_FAKE_SERVICE_CONNECTION_H_
#define CHROME_SERVICES_MACHINE_LEARNING_PUBLIC_CPP_TEST_SUPPORT_FAKE_SERVICE_CONNECTION_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "chrome/services/machine_learning/public/cpp/service_connection.h"
#include "chrome/services/machine_learning/public/mojom/decision_tree.mojom.h"
#include "chrome/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace machine_learning {
namespace testing {

// Fake implementation of machine_learning::ServiceConnection.
// - Replaces the actual ServiceConnection singleton on initialization.
// - Handles LoadDecisionTreeModel by returning the value specified by a
//   previous call to SetLoadModelResults. Binds to itself if result is set to
//   kOk.
// - Handles DecisionTreePredictor::Predict by returning the values specified by
//   a previous call to SetDecisionTreePredictionResult.
class FakeServiceConnection : public ServiceConnection,
                              public mojom::MachineLearningService,
                              public mojom::DecisionTreePredictor {
 public:
  FakeServiceConnection();
  ~FakeServiceConnection() override;

  FakeServiceConnection(const FakeServiceConnection&) = delete;
  FakeServiceConnection& operator=(const FakeServiceConnection&) = delete;

  // Runs all scheduled callbacks and removes them from schedule.
  void RunScheduledCalls();

  // Sets the return value for model loading.
  void SetLoadModelResult(mojom::LoadModelResult result);

  // Sets the return value for decision tree prediction.
  void SetDecisionTreePredictionResult(
      mojom::DecisionTreePredictionResult result,
      double prediction_score);

  // Whether the service is running.
  bool is_service_running() const;

  // ServiceConnection implementations.
  mojom::MachineLearningService* GetService() override;
  void ResetServiceForTesting() override;
  void LoadDecisionTreeModel(
      mojom::DecisionTreeModelSpecPtr spec,
      mojo::PendingReceiver<mojom::DecisionTreePredictor> receiver,
      mojom::MachineLearningService::LoadDecisionTreeCallback callback)
      override;

  // Sets whether calls are handled async for testing purposes.
  void SetAsyncModeForTesting(bool is_async);

 private:
  // Store |callback| to be run when RunScheduledCalls is invoked.
  void ScheduleCall(base::OnceClosure callback);

  // mojom::MachineLearningService implementations.
  void LoadDecisionTree(
      mojom::DecisionTreeModelSpecPtr spec,
      mojo::PendingReceiver<mojom::DecisionTreePredictor> receiver,
      LoadDecisionTreeCallback callback) override;

  // Callback used to handle LoadDecisionTree calls.
  void HandleLoadDecisionTree(
      mojo::PendingReceiver<mojom::DecisionTreePredictor> receiver,
      LoadDecisionTreeCallback callback);

  // mojom::DecisionTreePredictor implementations.
  void Predict(const base::flat_map<std::string, float>& model_features,
               PredictCallback callback) override;

  // Callback used to handle Predict calls.
  void HandleDecisionTreePredict(PredictCallback callback);

  bool is_service_running_ = false;
  bool is_async_ = true;

  mojo::ReceiverSet<mojom::DecisionTreePredictor> decision_tree_receivers_;
  std::vector<base::OnceClosure> pending_calls_;
  mojom::LoadModelResult load_model_result_ =
      mojom::LoadModelResult::kLoadModelError;
  mojom::DecisionTreePredictionResult decision_tree_prediction_result_ =
      mojom::DecisionTreePredictionResult::kUnknown;
  double decision_tree_prediction_score_ = 0.0;
};

}  // namespace testing
}  // namespace machine_learning

#endif  // CHROME_SERVICES_MACHINE_LEARNING_PUBLIC_CPP_TEST_SUPPORT_FAKE_SERVICE_CONNECTION_H_
