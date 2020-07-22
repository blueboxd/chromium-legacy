// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/machine_learning/public/cpp/service_connection.h"

#include <utility>

#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/services/machine_learning/public/mojom/decision_tree.mojom.h"
#include "chrome/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "content/public/browser/service_process_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace machine_learning {

namespace {

// The amount of idle time to tolerate on a ML Service connection. If the
// Service is unused for this period of time, the underlying service process
// may be killed and only restarted once needed again.
constexpr base::TimeDelta kServiceProcessIdleTimeout{
    base::TimeDelta::FromSeconds(30)};

// Actual implementation of |ServiceConnection|
// TODO(crbug/1102425): Add a browser test to actually test the implementation
// after hooked to Optimization Guide.
class ServiceConnectionImpl : public ServiceConnection {
 public:
  ServiceConnectionImpl();
  ~ServiceConnectionImpl() override = default;

  ServiceConnectionImpl(const ServiceConnectionImpl&) = delete;
  ServiceConnectionImpl& operator=(const ServiceConnectionImpl&) = delete;

  void LoadDecisionTreeModel(
      mojom::DecisionTreeModelSpecPtr spec,
      mojo::PendingReceiver<mojom::DecisionTreePredictor> receiver,
      mojom::MachineLearningService::LoadDecisionTreeCallback callback)
      override;

  mojom::MachineLearningService* GetService() override;

  void ResetServiceForTesting() override;

 private:
  mojo::Remote<mojom::MachineLearningService> machine_learning_service_;

  SEQUENCE_CHECKER(sequence_checker_);
};

ServiceConnectionImpl::ServiceConnectionImpl() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void ServiceConnectionImpl::LoadDecisionTreeModel(
    mojom::DecisionTreeModelSpecPtr spec,
    mojo::PendingReceiver<mojom::DecisionTreePredictor> receiver,
    mojom::MachineLearningService::LoadDecisionTreeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  GetService()->LoadDecisionTree(std::move(spec), std::move(receiver),
                                 std::move(callback));
}

mojom::MachineLearningService* ServiceConnectionImpl::GetService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!machine_learning_service_) {
    content::ServiceProcessHost::Launch(
        machine_learning_service_.BindNewPipeAndPassReceiver(),
        content::ServiceProcessHost::Options()
            .WithDisplayName("Machine Learning Service")
            .Pass());

    machine_learning_service_.reset_on_disconnect();
    machine_learning_service_.reset_on_idle_timeout(kServiceProcessIdleTimeout);
  }
  return machine_learning_service_.get();
}

void ServiceConnectionImpl::ResetServiceForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  machine_learning_service_.reset();
}

static ServiceConnection* g_fake_service_connection = nullptr;

}  // namespace

// static
ServiceConnection* ServiceConnection::GetInstance() {
  if (g_fake_service_connection)
    return g_fake_service_connection;

  static base::NoDestructor<ServiceConnectionImpl> service_connection;
  return service_connection.get();
}

// static
void ServiceConnection::SetServiceConnectionForTesting(
    ServiceConnection* service_connection) {
  g_fake_service_connection = service_connection;
}

}  // namespace machine_learning
