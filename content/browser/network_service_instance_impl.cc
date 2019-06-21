// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/network_service_instance_impl.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/deferred_sequenced_task_runner.h"
#include "base/environment.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "build/build_config.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/network_service_client.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/system_connector.h"
#include "content/public/common/network_service_util.h"
#include "content/public/common/service_names.mojom.h"
#include "net/log/net_log_util.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/mojom/net_log.mojom.h"
#include "services/network/public/mojom/network_change_manager.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace content {

namespace {

#if defined(OS_POSIX)
// Environment variable pointing to credential cache file.
constexpr char kKrb5CCEnvName[] = "KRB5CCNAME";
// Environment variable pointing to Kerberos config file.
constexpr char kKrb5ConfEnvName[] = "KRB5_CONFIG";
#endif

bool g_force_create_network_service_directly = false;
network::mojom::NetworkServicePtr* g_network_service_ptr = nullptr;
network::NetworkConnectionTracker* g_network_connection_tracker;

std::unique_ptr<network::NetworkService>& GetLocalNetworkService() {
  static base::NoDestructor<
      base::SequenceLocalStorageSlot<std::unique_ptr<network::NetworkService>>>
      service;
  return service->GetOrCreateValue();
}

network::mojom::NetworkServiceParamsPtr CreateNetworkServiceParams() {
  network::mojom::NetworkServiceParamsPtr network_service_params =
      network::mojom::NetworkServiceParams::New();
  network_service_params->initial_connection_type =
      network::mojom::ConnectionType(
          net::NetworkChangeNotifier::GetConnectionType());
  network_service_params->initial_connection_subtype =
      network::mojom::ConnectionSubtype(
          net::NetworkChangeNotifier::GetConnectionSubtype());

#if defined(OS_POSIX)
  // Send Kerberos environment variables to the network service.
  if (IsOutOfProcessNetworkService()) {
    std::unique_ptr<base::Environment> env(base::Environment::Create());
    std::string value;
    if (env->HasVar(kKrb5CCEnvName)) {
      env->GetVar(kKrb5CCEnvName, &value);
      network_service_params->environment.push_back(
          network::mojom::EnvironmentVariable::New(kKrb5CCEnvName, value));
    }
    if (env->HasVar(kKrb5ConfEnvName)) {
      env->GetVar(kKrb5ConfEnvName, &value);
      network_service_params->environment.push_back(
          network::mojom::EnvironmentVariable::New(kKrb5ConfEnvName, value));
    }
  }
#endif
  return network_service_params;
}

void CreateNetworkServiceOnIO(network::mojom::NetworkServiceRequest request) {
  if (GetLocalNetworkService()) {
    // GetNetworkServiceImpl() was already called and created the object, so
    // just bind it.
    GetLocalNetworkService()->Bind(std::move(request));
    return;
  }

  GetLocalNetworkService() = std::make_unique<network::NetworkService>(
      nullptr, std::move(request), GetContentClient()->browser()->GetNetLog());
}

void BindNetworkChangeManagerRequest(
    network::mojom::NetworkChangeManagerRequest request) {
  GetNetworkService()->GetNetworkChangeManager(std::move(request));
}

base::CallbackList<void()>& GetCrashHandlersList() {
  static base::NoDestructor<base::CallbackList<void()>> s_list;
  return *s_list;
}

void OnNetworkServiceCrash() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(g_network_service_ptr);
  DCHECK(g_network_service_ptr->is_bound());
  DCHECK(g_network_service_ptr->encountered_error());
  GetCrashHandlersList().Notify();
}

}  // namespace

network::mojom::NetworkService* GetNetworkService() {
  service_manager::Connector* connector = nullptr;
  if (base::FeatureList::IsEnabled(network::features::kNetworkService) &&
      GetSystemConnector() &&  // null in unit tests.
      !g_force_create_network_service_directly) {
    connector = GetSystemConnector();
  }
  return GetNetworkServiceFromConnector(connector);
}

CONTENT_EXPORT network::mojom::NetworkService* GetNetworkServiceFromConnector(
    service_manager::Connector* connector) {
  const bool is_network_service_enabled =
      base::FeatureList::IsEnabled(network::features::kNetworkService);
  // The DCHECK for thread is only done without network service enabled. This is
  // because the connector and the pre-existing |g_network_service_ptr| are
  // bound to the right thread in the network service case, and this allows
  // Android to instantiate the NetworkService before UI thread is promoted to
  // BrowserThread::UI.
  if (!is_network_service_enabled)
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!g_network_service_ptr)
    g_network_service_ptr = new network::mojom::NetworkServicePtr;
  static NetworkServiceClient* g_client;
  if (!g_network_service_ptr->is_bound() ||
      g_network_service_ptr->encountered_error()) {
    if (GetContentClient()->browser()->IsShuttingDown()) {
      // This happens at system shutdown, since in other scenarios the network
      // process would only be torn down once the message loop stopped running.
      // We don't want to want to start the network service again so just create
      // message pipe that's not bound to stop consumers from requesting
      // creation of the service.
      auto request = mojo::MakeRequest(g_network_service_ptr);
      auto leaked_pipe = request.PassMessagePipe().release();
    } else {
      if (is_network_service_enabled && connector) {
        connector->BindInterface(mojom::kNetworkServiceName,
                                 g_network_service_ptr);
        g_network_service_ptr->set_connection_error_handler(
            base::BindOnce(&OnNetworkServiceCrash));
      } else {
        base::PostTaskWithTraits(
            FROM_HERE, {BrowserThread::IO},
            base::BindOnce(CreateNetworkServiceOnIO,
                           mojo::MakeRequest(g_network_service_ptr)));
      }

      network::mojom::NetworkServiceClientPtr client_ptr;
      auto client_request = mojo::MakeRequest(&client_ptr);
      // Call SetClient before creating NetworkServiceClient, as the latter
      // might make requests to NetworkService that depend on initialization.
      (*g_network_service_ptr)
          ->SetClient(std::move(client_ptr), CreateNetworkServiceParams());

      delete g_client;  // In case we're recreating the network service.
      g_client = new NetworkServiceClient(std::move(client_request));

      const base::CommandLine* command_line =
          base::CommandLine::ForCurrentProcess();
      if (is_network_service_enabled) {
        if (command_line->HasSwitch(network::switches::kLogNetLog)) {
          base::FilePath log_path =
              command_line->GetSwitchValuePath(network::switches::kLogNetLog);

          base::DictionaryValue client_constants =
              GetContentClient()->GetNetLogConstants();

          base::File file(log_path, base::File::FLAG_CREATE_ALWAYS |
                                        base::File::FLAG_WRITE);
          if (!file.IsValid()) {
            LOG(ERROR) << "Failed opening: " << log_path.value();
          } else {
            net::NetLogCaptureMode capture_mode =
                net::GetNetCaptureModeFromCommandLine(
                    *command_line, network::switches::kNetLogCaptureMode);

            (*g_network_service_ptr)
                ->StartNetLog(std::move(file), capture_mode,
                              std::move(client_constants));
          }
        }
      }

      if (command_line->HasSwitch(network::switches::kSSLKeyLogFile)) {
        base::FilePath log_path =
            command_line->GetSwitchValuePath(network::switches::kSSLKeyLogFile);
        LOG_IF(WARNING, log_path.empty())
            << "ssl-key-log-file argument missing";
        if (!log_path.empty())
          (*g_network_service_ptr)->SetSSLKeyLogFile(log_path);
      }

      std::unique_ptr<base::Environment> env(base::Environment::Create());
      std::string env_str;
      if (env->GetVar("SSLKEYLOGFILE", &env_str)) {
#if defined(OS_WIN)
        // base::Environment returns environment variables in UTF-8 on Windows.
        base::FilePath log_path(base::UTF8ToUTF16(env_str));
#else
        base::FilePath log_path(env_str);
#endif
        if (!log_path.empty())
          (*g_network_service_ptr)->SetSSLKeyLogFile(log_path);
      }

      GetContentClient()->browser()->OnNetworkServiceCreated(
          g_network_service_ptr->get());
    }
  }
  return g_network_service_ptr->get();
}

std::unique_ptr<base::CallbackList<void()>::Subscription>
RegisterNetworkServiceCrashHandler(base::RepeatingClosure handler) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!handler.is_null());

  if (base::FeatureList::IsEnabled(network::features::kNetworkService))
    return GetCrashHandlersList().Add(std::move(handler));

  return nullptr;
}

network::NetworkService* GetNetworkServiceImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!base::FeatureList::IsEnabled(network::features::kNetworkService));
  if (!GetLocalNetworkService()) {
    GetLocalNetworkService() = std::make_unique<network::NetworkService>(
        nullptr, nullptr, GetContentClient()->browser()->GetNetLog());
  }

  return GetLocalNetworkService().get();
}

#if defined(OS_CHROMEOS)
net::NetworkChangeNotifier* GetNetworkChangeNotifier() {
  return BrowserMainLoop::GetInstance()->network_change_notifier();
}
#endif

void FlushNetworkServiceInstanceForTesting() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (g_network_service_ptr)
    g_network_service_ptr->FlushForTesting();
}

network::NetworkConnectionTracker* GetNetworkConnectionTracker() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         !BrowserThread::IsThreadInitialized(BrowserThread::UI));
  if (!g_network_connection_tracker) {
    g_network_connection_tracker = new network::NetworkConnectionTracker(
        base::BindRepeating(&BindNetworkChangeManagerRequest));
  }
  return g_network_connection_tracker;
}

void GetNetworkConnectionTrackerFromUIThread(
    base::OnceCallback<void(network::NetworkConnectionTracker*)> callback) {
  // TODO(fdoray): Investigate why this is needed. The IO thread is supposed to
  // be initialized by the time the UI thread starts running tasks.
  //
  // GetNetworkConnectionTracker() will call CreateNetworkServiceOnIO(). Here it
  // makes sure the IO thread is running when CreateNetworkServiceOnIO() is
  // called.
  if (!content::BrowserThread::IsThreadInitialized(
          content::BrowserThread::IO)) {
    // IO thread is not yet initialized. Try again in the next message pump.
    bool task_posted = base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&GetNetworkConnectionTrackerFromUIThread,
                                  std::move(callback)));
    DCHECK(task_posted);
    return;
  }

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {BrowserThread::UI, base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&GetNetworkConnectionTracker), std::move(callback));
}

network::NetworkConnectionTrackerAsyncGetter
CreateNetworkConnectionTrackerAsyncGetter() {
  return base::BindRepeating(&content::GetNetworkConnectionTrackerFromUIThread);
}

void SetNetworkConnectionTrackerForTesting(
    network::NetworkConnectionTracker* network_connection_tracker) {
  if (g_network_connection_tracker != network_connection_tracker) {
    DCHECK(!g_network_connection_tracker || !network_connection_tracker);
    g_network_connection_tracker = network_connection_tracker;
  }
}

scoped_refptr<base::DeferredSequencedTaskRunner> GetNetworkTaskRunner() {
  DCHECK(IsInProcessNetworkService());
  static base::NoDestructor<scoped_refptr<base::DeferredSequencedTaskRunner>>
      instance(new base::DeferredSequencedTaskRunner());
  return instance->get();
}

void ForceCreateNetworkServiceDirectlyForTesting() {
  g_force_create_network_service_directly = true;
}

void ResetNetworkServiceForTesting() {
  delete g_network_service_ptr;
  g_network_service_ptr = nullptr;
}

}  // namespace content
