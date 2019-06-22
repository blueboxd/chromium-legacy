// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/web_engine_browser_context.h"

#include <memory>
#include <utility>

#include "base/base_paths_fuchsia.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/task/post_task.h"
#include "components/keyed_service/core/simple_key_map.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_context.h"
#include "fuchsia/engine/browser/web_engine_net_log.h"
#include "fuchsia/engine/browser/web_engine_permission_manager.h"
#include "fuchsia/engine/browser/web_engine_url_request_context_getter.h"
#include "fuchsia/engine/common.h"
#include "net/url_request/url_request_context.h"
#include "services/network/public/cpp/network_switches.h"

class WebEngineBrowserContext::ResourceContext
    : public content::ResourceContext {
 public:
  ResourceContext() = default;
  ~ResourceContext() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ResourceContext);
};

std::unique_ptr<WebEngineNetLog> CreateNetLog() {
  std::unique_ptr<WebEngineNetLog> result;

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(network::switches::kLogNetLog)) {
    base::FilePath log_path =
        command_line->GetSwitchValuePath(network::switches::kLogNetLog);
    result = std::make_unique<WebEngineNetLog>(log_path);
  }

  return result;
}

WebEngineBrowserContext::WebEngineBrowserContext(bool force_incognito)
    : net_log_(CreateNetLog()), resource_context_(new ResourceContext()) {
  if (!force_incognito) {
    base::PathService::Get(base::DIR_APP_DATA, &data_dir_path_);
    if (!base::PathExists(data_dir_path_)) {
      // Run in incognito mode if /data doesn't exist.
      data_dir_path_.clear();
    }
  }
  simple_factory_key_ =
      std::make_unique<SimpleFactoryKey>(GetPath(), IsOffTheRecord());
  SimpleKeyMap::GetInstance()->Associate(this, simple_factory_key_.get());
  BrowserContext::Initialize(this, data_dir_path_);
}

WebEngineBrowserContext::~WebEngineBrowserContext() {
  SimpleKeyMap::GetInstance()->Dissociate(this);
  NotifyWillBeDestroyed(this);

  if (resource_context_) {
    content::BrowserThread::DeleteSoon(content::BrowserThread::IO, FROM_HERE,
                                       std::move(resource_context_));
  }

  ShutdownStoragePartitions();
}

std::unique_ptr<content::ZoomLevelDelegate>
WebEngineBrowserContext::CreateZoomLevelDelegate(
    const base::FilePath& partition_path) {
  return nullptr;
}

base::FilePath WebEngineBrowserContext::GetPath() {
  return data_dir_path_;
}

bool WebEngineBrowserContext::IsOffTheRecord() {
  return data_dir_path_.empty();
}

content::ResourceContext* WebEngineBrowserContext::GetResourceContext() {
  return resource_context_.get();
}

content::DownloadManagerDelegate*
WebEngineBrowserContext::GetDownloadManagerDelegate() {
  NOTIMPLEMENTED();
  return nullptr;
}

content::BrowserPluginGuestManager* WebEngineBrowserContext::GetGuestManager() {
  return nullptr;
}

storage::SpecialStoragePolicy*
WebEngineBrowserContext::GetSpecialStoragePolicy() {
  return nullptr;
}

content::PushMessagingService*
WebEngineBrowserContext::GetPushMessagingService() {
  return nullptr;
}

content::SSLHostStateDelegate*
WebEngineBrowserContext::GetSSLHostStateDelegate() {
  return nullptr;
}

content::PermissionControllerDelegate*
WebEngineBrowserContext::GetPermissionControllerDelegate() {
  if (!permission_manager_)
    permission_manager_ = std::make_unique<WebEnginePermissionManager>();
  return permission_manager_.get();
}

content::ClientHintsControllerDelegate*
WebEngineBrowserContext::GetClientHintsControllerDelegate() {
  return nullptr;
}

content::BackgroundFetchDelegate*
WebEngineBrowserContext::GetBackgroundFetchDelegate() {
  return nullptr;
}

content::BackgroundSyncController*
WebEngineBrowserContext::GetBackgroundSyncController() {
  return nullptr;
}

content::BrowsingDataRemoverDelegate*
WebEngineBrowserContext::GetBrowsingDataRemoverDelegate() {
  return nullptr;
}

net::URLRequestContextGetter* WebEngineBrowserContext::CreateRequestContext(
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  DCHECK(!url_request_getter_);
  url_request_getter_ = new WebEngineURLRequestContextGetter(
      base::CreateSingleThreadTaskRunnerWithTraits(
          {content::BrowserThread::IO}),
      net_log_.get(), std::move(*protocol_handlers),
      std::move(request_interceptors), data_dir_path_);
  return url_request_getter_.get();
}

net::URLRequestContextGetter*
WebEngineBrowserContext::CreateMediaRequestContext() {
  DCHECK(url_request_getter_.get());
  return url_request_getter_.get();
}
