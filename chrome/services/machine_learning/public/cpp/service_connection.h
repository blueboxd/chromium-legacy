// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_MACHINE_LEARNING_PUBLIC_CPP_SERVICE_CONNECTION_H_
#define CHROME_SERVICES_MACHINE_LEARNING_PUBLIC_CPP_SERVICE_CONNECTION_H_

#include "chrome/services/machine_learning/public/mojom/decision_tree.mojom.h"
#include "chrome/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace machine_learning {

// Singleton class that provides a lazily-launched connection to the Chrome ML
// Service.
class ServiceConnection {
 public:
  // Retrieves the singleton instance.
  static ServiceConnection* GetInstance();

  // Overrides the singleton instance for testing purposes.
  static void SetServiceConnectionForTesting(
      ServiceConnection* service_connection);

  // Returns a handle to the Service endpoint. This will reuse any connection to
  // the Chrome ML Service if already established, otherwise a new Service
  // process will be launched.
  virtual mojom::MachineLearningService* GetService() = 0;

  // Resets the service receiver such that the service process is forced to
  // terminate. For testing purposes only.
  virtual void ResetServiceForTesting() = 0;

  // Initiates the loading of the Decision Tree model specified in |spec|,
  // binding a DecisionTreePredictor implementation to |receiver|.
  // This will check if a connection to the ML Service exists and initiate the
  // connection if necessary.
  virtual void LoadDecisionTreeModel(
      mojom::DecisionTreeModelSpecPtr spec,
      mojo::PendingReceiver<mojom::DecisionTreePredictor> receiver,
      mojom::MachineLearningService::LoadDecisionTreeCallback callback) = 0;

 protected:
  ServiceConnection() = default;
  virtual ~ServiceConnection() = default;
};

}  // namespace machine_learning

#endif  // CHROME_SERVICES_MACHINE_LEARNING_PUBLIC_CPP_SERVICE_CONNECTION_H_
