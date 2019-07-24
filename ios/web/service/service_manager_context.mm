// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/service/service_manager_context.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/process/process_handle.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "ios/web/public/service/service_manager_connection.h"
#include "ios/web/public/service/service_names.mojom.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#include "ios/web/public/web_client.h"
#include "ios/web/service/service_manager_connection_impl.h"
#import "ios/web/service/web_browser_manifest.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/constants.h"
#include "services/service_manager/public/cpp/manifest.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/service_manager/service_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

namespace {

struct ManifestInfo {
  const char* name;
  int resource_id;
};

service_manager::Manifest GetWebSystemManifest() {
  // TODO(crbug.com/961869): This is a bit of a temporary hack so that we can
  // make the global service instance a singleton. For now we just mirror the
  // per-BrowserState manifest (formerly also used for the global singleton
  // instance), sans packaged services, since those are only meant to be tied to
  // a BrowserState. The per-BrowserState service should go away soon, and then
  // this can be removed.
  service_manager::Manifest manifest = GetWebBrowserManifest();
  manifest.service_name = mojom::kSystemServiceName;
  manifest.packaged_services.clear();
  manifest.options.instance_sharing_policy =
      service_manager::Manifest::InstanceSharingPolicy::kSingleton;
  return manifest;
}

using ServiceRunner = base::RepeatingCallback<void(
    const service_manager::Identity&,
    mojo::PendingReceiver<service_manager::mojom::Service> receiver)>;

class BrowserServiceManagerDelegate
    : public service_manager::ServiceManager::Delegate {
 public:
  BrowserServiceManagerDelegate(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      ServiceRunner service_runner)
      : task_runner_(std::move(task_runner)),
        service_runner_(std::move(service_runner)) {}
  ~BrowserServiceManagerDelegate() override = default;

  bool RunBuiltinServiceInstanceInCurrentProcess(
      const service_manager::Identity& identity,
      mojo::PendingReceiver<service_manager::mojom::Service> receiver)
      override {
    task_runner_->PostTask(FROM_HERE, base::BindOnce(service_runner_, identity,
                                                     std::move(receiver)));
    return true;
  }

  std::unique_ptr<service_manager::ServiceProcessHost>
  CreateProcessHostForBuiltinServiceInstance(
      const service_manager::Identity& identity) override {
    return nullptr;
  }

  std::unique_ptr<service_manager::ServiceProcessHost>
  CreateProcessHostForServiceExecutable(
      const base::FilePath& executable_path) override {
    return nullptr;
  }

 private:
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  const ServiceRunner service_runner_;

  DISALLOW_COPY_AND_ASSIGN(BrowserServiceManagerDelegate);
};

}  // namespace

// State which lives on the IO thread and drives the ServiceManager.
class ServiceManagerContext::InProcessServiceManagerContext
    : public base::RefCountedThreadSafe<InProcessServiceManagerContext> {
 public:
  InProcessServiceManagerContext() {}

  void Start(mojo::PendingRemote<service_manager::mojom::Service> system_remote,
             std::vector<service_manager::Manifest> manifests,
             ServiceRunner service_runner) {
    base::PostTaskWithTraits(
        FROM_HERE, {WebThread::IO},
        base::BindOnce(&InProcessServiceManagerContext::StartOnIOThread, this,
                       std::move(manifests), std::move(system_remote),
                       base::SequencedTaskRunnerHandle::Get(),
                       std::move(service_runner)));
  }

  void ShutDown() {
    base::PostTaskWithTraits(
        FROM_HERE, {WebThread::IO},
        base::BindOnce(&InProcessServiceManagerContext::ShutDownOnIOThread,
                       this));
  }

 private:
  friend class base::RefCountedThreadSafe<InProcessServiceManagerContext>;

  ~InProcessServiceManagerContext() = default;

  // Creates the ServiceManager and registers the packaged services service
  // with it, connecting the other end of the packaged services serviceto
  // |packaged_services_service_info|.
  void StartOnIOThread(
      std::vector<service_manager::Manifest> manifests,
      mojo::PendingRemote<service_manager::mojom::Service> system_remote,
      scoped_refptr<base::SequencedTaskRunner> service_task_runner,
      ServiceRunner service_runner) {
    service_manager_ = std::make_unique<service_manager::ServiceManager>(
        manifests,
        std::make_unique<BrowserServiceManagerDelegate>(
            std::move(service_task_runner), std::move(service_runner)));

    mojo::Remote<service_manager::mojom::ProcessMetadata> metadata;
    service_manager_->RegisterService(
        service_manager::Identity(mojom::kSystemServiceName,
                                  service_manager::kSystemInstanceGroup,
                                  base::Token{}, base::Token::CreateRandom()),
        std::move(system_remote), metadata.BindNewPipeAndPassReceiver());
    metadata->SetPID(base::GetCurrentProcId());
  }

  void ShutDownOnIOThread() { service_manager_.reset(); }

  std::unique_ptr<service_manager::ServiceManager> service_manager_;

  DISALLOW_COPY_AND_ASSIGN(InProcessServiceManagerContext);
};

ServiceManagerContext::ServiceManagerContext() {
  std::vector<service_manager::Manifest> manifests = {GetWebSystemManifest(),
                                                      GetWebBrowserManifest()};
  for (auto& manifest : GetWebClient()->GetExtraServiceManifests())
    manifests.push_back(std::move(manifest));

  mojo::PendingRemote<service_manager::mojom::Service> system_remote;
  ServiceManagerConnection::Set(ServiceManagerConnection::Create(
      system_remote.InitWithNewPipeAndPassReceiver(),
      base::CreateSingleThreadTaskRunnerWithTraits({WebThread::IO})));
  auto* system_connection = ServiceManagerConnection::Get();

  in_process_context_ = base::MakeRefCounted<InProcessServiceManagerContext>();
  in_process_context_->Start(
      std::move(system_remote), std::move(manifests),
      base::BindRepeating(&ServiceManagerContext::RunService,
                          weak_ptr_factory_.GetWeakPtr()));
  system_connection->Start();
}

ServiceManagerContext::~ServiceManagerContext() {
  // NOTE: The in-process ServiceManager MUST be destroyed before the browser
  // process-wide ServiceManagerConnection. Otherwise it's possible for the
  // ServiceManager to receive connection requests for service:ios_web_browser
  // which it may attempt to service by launching a new instance of the browser.
  if (in_process_context_)
    in_process_context_->ShutDown();
  if (ServiceManagerConnection::Get())
    ServiceManagerConnection::Destroy();
}

void ServiceManagerContext::RunService(
    const service_manager::Identity& identity,
    mojo::PendingReceiver<service_manager::mojom::Service> receiver) {
  std::unique_ptr<service_manager::Service> service =
      GetWebClient()->HandleServiceRequest(identity.name(),
                                           std::move(receiver));
  if (!service) {
    LOG(ERROR) << "Ignoring unhandled request for service: " << identity.name();
    return;
  }

  auto* raw_service = service.get();
  service->set_termination_closure(
      base::BindOnce(&ServiceManagerContext::OnServiceQuit,
                     base::Unretained(this), raw_service));
  running_services_.emplace(raw_service, std::move(service));
}

void ServiceManagerContext::OnServiceQuit(service_manager::Service* service) {
  running_services_.erase(service);
}

}  // namespace web
