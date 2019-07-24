// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/service/service_manager_connection_impl.h"

#include <queue>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/message_loop/message_loop_current.h"
#include "base/no_destructor.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ios/web/public/thread/web_thread.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/mojom/constants.mojom.h"

namespace web {
namespace {

std::unique_ptr<ServiceManagerConnection>& GetConnectionForProcess() {
  static base::NoDestructor<std::unique_ptr<ServiceManagerConnection>>
      connection_for_process;
  return *connection_for_process;
}

}  // namespace

// A ref-counted object which owns the IO thread state of a
// ServiceManagerConnectionImpl. This includes Service bindings.
class ServiceManagerConnectionImpl::IOThreadContext
    : public base::RefCountedThreadSafe<IOThreadContext>,
      public service_manager::Service {
 public:
  IOThreadContext(service_manager::mojom::ServiceRequest service_request,
                  scoped_refptr<base::SequencedTaskRunner> io_task_runner,
                  service_manager::mojom::ConnectorRequest connector_request)
      : pending_service_request_(std::move(service_request)),
        io_task_runner_(io_task_runner),
        pending_connector_request_(std::move(connector_request)),
        weak_factory_(this) {
    // This will be reattached by any of the IO thread functions on first call.
    io_thread_checker_.DetachFromThread();
  }

  // Safe to call from any thread.
  void Start(base::OnceClosure stop_callback,
             const ServiceManagerConnection::ServiceRequestHandler&
                 default_request_handler) {
    DCHECK(!started_);

    started_ = true;
    callback_task_runner_ = base::ThreadTaskRunnerHandle::Get();
    stop_callback_ = std::move(stop_callback);
    default_request_handler_ = default_request_handler;
    io_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&IOThreadContext::StartOnIOThread, this));
  }

  // Safe to call from whichever thread called Start() (or may have called
  // Start()). Must be called before IO thread shutdown.
  void ShutDown() {
    if (!started_)
      return;

    bool posted = io_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&IOThreadContext::ShutDownOnIOThread, this));
    DCHECK(posted);
  }

 private:
  friend class base::RefCountedThreadSafe<IOThreadContext>;

  class MessageLoopObserver
      : public base::MessageLoopCurrent::DestructionObserver {
   public:
    explicit MessageLoopObserver(base::WeakPtr<IOThreadContext> context)
        : context_(context) {
      base::MessageLoopCurrent::Get()->AddDestructionObserver(this);
    }

    ~MessageLoopObserver() override {
      base::MessageLoopCurrent::Get()->RemoveDestructionObserver(this);
    }

    void ShutDown() {
      if (!is_active_)
        return;

      // The call into |context_| below may reenter ShutDown(), hence we set
      // |is_active_| to false here.
      is_active_ = false;
      if (context_)
        context_->ShutDownOnIOThread();

      delete this;
    }

   private:
    void WillDestroyCurrentMessageLoop() override {
      DCHECK(is_active_);
      ShutDown();
    }

    bool is_active_ = true;
    base::WeakPtr<IOThreadContext> context_;

    DISALLOW_COPY_AND_ASSIGN(MessageLoopObserver);
  };

  ~IOThreadContext() override {}

  void StartOnIOThread() {
    // Should bind |io_thread_checker_| to the context's thread.
    DCHECK(io_thread_checker_.CalledOnValidThread());
    DCHECK(!service_binding_);
    service_binding_ = std::make_unique<service_manager::ServiceBinding>(
        this, std::move(pending_service_request_));
    service_binding_->GetConnector()->BindConnectorRequest(
        std::move(pending_connector_request_));

    // MessageLoopObserver owns itself.
    message_loop_observer_ =
        new MessageLoopObserver(weak_factory_.GetWeakPtr());
  }

  void ShutDownOnIOThread() {
    DCHECK(io_thread_checker_.CalledOnValidThread());

    weak_factory_.InvalidateWeakPtrs();

    // Note that this method may be invoked by MessageLoopObserver observing
    // MessageLoop destruction. In that case, this call to ShutDown is
    // effectively a no-op. In any case it's safe.
    if (message_loop_observer_) {
      message_loop_observer_->ShutDown();
      message_loop_observer_ = nullptr;
    }

    // Resetting the ServiceContext below may otherwise release the last
    // reference to this IOThreadContext. We keep it alive until the stack
    // unwinds.
    scoped_refptr<IOThreadContext> keepalive(this);

    service_binding_.reset();
  }

  // service_manager::Service:
  void CreatePackagedServiceInstance(
      const std::string& service_name,
      mojo::PendingReceiver<service_manager::mojom::Service> receiver,
      CreatePackagedServiceInstanceCallback callback) override {
    DCHECK(io_thread_checker_.CalledOnValidThread());
    callback_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(default_request_handler_, service_name,
                                  service_manager::mojom::ServiceRequest(
                                      std::move(receiver))));
    std::move(callback).Run(base::ProcessId());
  }

  void OnDisconnected() override {
    callback_task_runner_->PostTask(FROM_HERE, std::move(stop_callback_));
  }

  base::ThreadChecker io_thread_checker_;
  bool started_ = false;

  // Temporary state established on construction and consumed on the IO thread
  // once the connection is started.
  service_manager::mojom::ServiceRequest pending_service_request_;
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
  service_manager::mojom::ConnectorRequest pending_connector_request_;

  // TaskRunner on which to run our owner's callbacks, i.e. the ones passed to
  // Start().
  scoped_refptr<base::SequencedTaskRunner> callback_task_runner_;

  // Callback to run if the service is stopped by the service manager.
  base::OnceClosure stop_callback_;

  ServiceManagerConnection::ServiceRequestHandler default_request_handler_;

  std::unique_ptr<service_manager::ServiceBinding> service_binding_;

  // Not owned.
  MessageLoopObserver* message_loop_observer_ = nullptr;

  base::WeakPtrFactory<IOThreadContext> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(IOThreadContext);
};

////////////////////////////////////////////////////////////////////////////////
// ServiceManagerConnection, public:

// static
void ServiceManagerConnection::Set(
    std::unique_ptr<ServiceManagerConnection> connection) {
  DCHECK_CURRENTLY_ON(WebThread::UI);
  DCHECK(!GetConnectionForProcess());
  GetConnectionForProcess() = std::move(connection);
}

// static
ServiceManagerConnection* ServiceManagerConnection::Get() {
  // WebThreads are not initialized in many unit tests. These tests also by
  // definition are not setting the global ServiceManagerConnection (since
  // otherwise the DCHECK in the above method would fire).
  DCHECK(!web::WebThread::IsThreadInitialized(web::WebThread::UI) ||
         web::WebThread::CurrentlyOn(web::WebThread::UI));
  return GetConnectionForProcess().get();
}

// static
void ServiceManagerConnection::Destroy() {
  DCHECK_CURRENTLY_ON(WebThread::UI);

  // This joins the service manager controller thread.
  GetConnectionForProcess().reset();
}

// static
std::unique_ptr<ServiceManagerConnection> ServiceManagerConnection::Create(
    service_manager::mojom::ServiceRequest request,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner) {
  return std::make_unique<ServiceManagerConnectionImpl>(std::move(request),
                                                        io_task_runner);
}

ServiceManagerConnection::~ServiceManagerConnection() {}

////////////////////////////////////////////////////////////////////////////////
// ServiceManagerConnectionImpl, public:

ServiceManagerConnectionImpl::ServiceManagerConnectionImpl(
    service_manager::mojom::ServiceRequest request,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner)
    : weak_factory_(this) {
  service_manager::mojom::ConnectorRequest connector_request;
  connector_ = service_manager::Connector::Create(&connector_request);
  context_ = new IOThreadContext(std::move(request), io_task_runner,
                                 std::move(connector_request));
}

ServiceManagerConnectionImpl::~ServiceManagerConnectionImpl() {
  context_->ShutDown();
}

////////////////////////////////////////////////////////////////////////////////
// ServiceManagerConnectionImpl, ServiceManagerConnection implementation:

void ServiceManagerConnectionImpl::Start() {
  context_->Start(
      base::BindOnce(&ServiceManagerConnectionImpl::OnConnectionLost,
                     weak_factory_.GetWeakPtr()),
      base::BindRepeating(
          &ServiceManagerConnectionImpl::OnUnhandledServiceRequest,
          weak_factory_.GetWeakPtr()));
}

service_manager::Connector* ServiceManagerConnectionImpl::GetConnector() {
  return connector_.get();
}

void ServiceManagerConnectionImpl::SetDefaultServiceRequestHandler(
    const ServiceRequestHandler& handler) {
  default_request_handler_ = handler;
}

void ServiceManagerConnectionImpl::OnConnectionLost() {}

void ServiceManagerConnectionImpl::GetInterface(
    service_manager::mojom::InterfaceProvider* provider,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle request_handle) {
  provider->GetInterface(interface_name, std::move(request_handle));
}

void ServiceManagerConnectionImpl::OnUnhandledServiceRequest(
    const std::string& service_name,
    service_manager::mojom::ServiceRequest request) {
  if (default_request_handler_)
    default_request_handler_.Run(service_name, std::move(request));
  else
    LOG(ERROR) << "Can't create service: " << service_name;
}

}  // namespace web
