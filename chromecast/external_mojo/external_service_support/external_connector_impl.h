// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_EXTERNAL_CONNECTOR_IMPL_H_
#define CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_EXTERNAL_CONNECTOR_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chromecast/external_mojo/external_service_support/external_connector.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromecast {
namespace external_service_support {

class ExternalConnectorImpl : public ExternalConnector {
 public:
  explicit ExternalConnectorImpl(const std::string& broker_path);
  explicit ExternalConnectorImpl(
      const std::string& broker_path,
      mojo::PendingRemote<external_mojo::mojom::ExternalConnector>
          connector_pending_remote);
  ~ExternalConnectorImpl() override;

  // ExternalConnector implementation:
  std::unique_ptr<base::CallbackList<void()>::Subscription>
  AddConnectionErrorCallback(base::RepeatingClosure callback) override;
  void RegisterService(const std::string& service_name,
                       ExternalService* service) override;
  void RegisterService(
      const std::string& service_name,
      mojo::PendingRemote<external_mojo::mojom::ExternalService> service_remote)
      override;
  void BindInterface(const std::string& service_name,
                     const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle interface_pipe,
                     bool async = true) override;
  std::unique_ptr<ExternalConnector> Clone() override;
  void SendChromiumConnectorRequest(
      mojo::ScopedMessagePipeHandle request) override;
  void QueryServiceList(
      base::OnceCallback<
          void(std::vector<
               chromecast::external_mojo::mojom::ExternalServiceInfoPtr>)>
          callback) override;

 private:
  void BindInterfaceImmediately(const std::string& service_name,
                                const std::string& interface_name,
                                mojo::ScopedMessagePipeHandle interface_pipe);
  void OnMojoDisconnect();
  bool BindConnectorIfNecessary();
  void InitializeBrokerConnection();
  void AttemptBrokerConnection();

  std::string broker_path_;

  mojo::Remote<external_mojo::mojom::ExternalConnector> connector_;

  base::CallbackList<void()> error_callbacks_;

  // If connecting to a broker, |connector_pending_receiver_for_broker_| is used
  // to keep |connector_| bound while waiting for the broker.
  mojo::PendingReceiver<external_mojo::mojom::ExternalConnector>
      connector_pending_receiver_for_broker_;

  // If cloned, |connector_pending_remote_from_clone_| is stored until an IO
  // operation is performed to ensure it happens on the correct sequence.
  mojo::PendingRemote<external_mojo::mojom::ExternalConnector>
      connector_pending_remote_from_clone_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ExternalConnectorImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ExternalConnectorImpl);
};

}  // namespace external_service_support
}  // namespace chromecast

#endif  // CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_EXTERNAL_CONNECTOR_IMPL_H_
