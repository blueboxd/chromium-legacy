// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_SERVICE_MANAGER_CONNECTION_H_
#define CONTENT_PUBLIC_COMMON_SERVICE_MANAGER_CONNECTION_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/sequenced_task_runner.h"
#include "content/common/content_export.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/mojom/connector.mojom-forward.h"
#include "services/service_manager/public/mojom/service.mojom-forward.h"

namespace service_manager {
class Connector;
}

namespace content {

// Encapsulates a connection to a //services/service_manager.
// Access a global instance on the thread the ServiceContext was bound by
// calling Holder::Get().
// Clients can add service_manager::Service implementations whose exposed
// interfaces
// will be exposed to inbound connections to this object's Service.
// Alternatively clients can define named services that will be constructed when
// requests for those service names are received.
// Clients must call any of the registration methods when receiving
// ContentBrowserClient::RegisterInProcessServices().
class CONTENT_EXPORT ServiceManagerConnection {
 public:
  using ServiceRequestHandler =
      base::RepeatingCallback<void(service_manager::mojom::ServiceRequest)>;
  using ServiceRequestHandlerWithCallback = base::RepeatingCallback<void(
      service_manager::mojom::ServiceRequest,
      service_manager::Service::CreatePackagedServiceInstanceCallback)>;
  using Factory =
      base::RepeatingCallback<std::unique_ptr<ServiceManagerConnection>(void)>;

  // Stores an instance of |connection| in TLS for the current process. Must be
  // called on the thread the connection was created on.
  static void SetForProcess(
      std::unique_ptr<ServiceManagerConnection> connection);

  // Returns the per-process instance, or nullptr if the Service Manager
  // connection has not yet been bound. Must be called on the thread the
  // connection was created on.
  static ServiceManagerConnection* GetForProcess();

  // Destroys the per-process instance. Must be called on the thread the
  // connection was created on.
  static void DestroyForProcess();

  virtual ~ServiceManagerConnection();

  // Sets the factory used to create the ServiceManagerConnection. This must be
  // called before the ServiceManagerConnection has been created.
  static void SetFactoryForTest(Factory* factory);

  // Creates a ServiceManagerConnection from |request|. The connection binds
  // its interfaces and accept new connections on |io_task_runner| only. Note
  // that no incoming connections are accepted until Start() is called.
  static std::unique_ptr<ServiceManagerConnection> Create(
      service_manager::mojom::ServiceRequest request,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner);

  // Begins accepting incoming connections.
  virtual void Start() = 0;

  // Stops accepting incoming connections. This happens asynchronously by
  // posting to the IO thread, and cannot be undone.
  virtual void Stop() = 0;

  // Returns the service_manager::Connector received via this connection's
  // Service
  // implementation. Use this to initiate connections as this object's Identity.
  virtual service_manager::Connector* GetConnector() = 0;

  // Sets a closure that is called when the connection is lost. Note that
  // connection may already have been closed, in which case |closure| will be
  // run immediately before returning from this function.
  virtual void SetConnectionLostClosure(base::OnceClosure closure) = 0;

  // Adds a generic ServiceRequestHandler for a given service name. This
  // will be used to satisfy any incoming calls to CreateService() which
  // reference the given name.
  virtual void AddServiceRequestHandler(
      const std::string& name,
      const ServiceRequestHandler& handler) = 0;

  // Similar to above but for registering handlers which want to communicate
  // additional information the process hosting the new service.
  virtual void AddServiceRequestHandlerWithCallback(
      const std::string& name,
      const ServiceRequestHandlerWithCallback& handler) = 0;

  // Sets a request handler to use if no registered handlers were interested in
  // an incoming service request. Must be called before |Start()|.
  using DefaultServiceRequestHandler =
      base::RepeatingCallback<void(const std::string& service_name,
                                   service_manager::mojom::ServiceRequest)>;
  virtual void SetDefaultServiceRequestHandler(
      const DefaultServiceRequestHandler& handler) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_SERVICE_MANAGER_CONNECTION_H_
