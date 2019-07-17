// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_navigation_handle_core.h"

#include <map>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "content/browser/appcache/appcache_host.h"
#include "content/browser/appcache/appcache_navigation_handle.h"
#include "content/browser/appcache/appcache_service_impl.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/loader/navigation_url_loader_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/child_process_host.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"
#include "third_party/blink/public/mojom/appcache/appcache_info.mojom.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"

namespace {

// Map of AppCache host id to the AppCacheNavigationHandleCore instance.
// Accessed on the IO thread only.
using AppCacheHandleMap =
    std::map<base::UnguessableToken, content::AppCacheNavigationHandleCore*>;
base::LazyInstance<AppCacheHandleMap>::DestructorAtExit g_appcache_handle_map =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace content {

AppCacheNavigationHandleCore::AppCacheNavigationHandleCore(
    ChromeAppCacheService* appcache_service,
    const base::UnguessableToken& appcache_host_id,
    int process_id)
    : appcache_service_(appcache_service),
      appcache_host_id_(appcache_host_id),
      process_id_(process_id) {
  // The AppCacheNavigationHandleCore is created on the UI thread but
  // should only be accessed from the IO thread afterwards.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!appcache_host_id_.is_empty());
}

AppCacheNavigationHandleCore::~AppCacheNavigationHandleCore() {
  DCHECK_CURRENTLY_ON(
      NavigationURLLoaderImpl::GetLoaderRequestControllerThreadID());
  precreated_host_.reset(nullptr);
  g_appcache_handle_map.Get().erase(appcache_host_id_);
}

void AppCacheNavigationHandleCore::Initialize() {
  DCHECK_CURRENTLY_ON(
      NavigationURLLoaderImpl::GetLoaderRequestControllerThreadID());
  DCHECK(precreated_host_.get() == nullptr);
  precreated_host_ = std::make_unique<AppCacheHost>(
      appcache_host_id_, process_id_, MSG_ROUTING_NONE, mojo::NullRemote(),
      GetAppCacheService());

  DCHECK(g_appcache_handle_map.Get().find(appcache_host_id_) ==
         g_appcache_handle_map.Get().end());
  g_appcache_handle_map.Get()[appcache_host_id_] = this;
}

// static
std::unique_ptr<AppCacheHost> AppCacheNavigationHandleCore::GetPrecreatedHost(
    const base::UnguessableToken& host_id) {
  DCHECK_CURRENTLY_ON(
      NavigationURLLoaderImpl::GetLoaderRequestControllerThreadID());
  auto index = g_appcache_handle_map.Get().find(host_id);
  if (index != g_appcache_handle_map.Get().end()) {
    AppCacheNavigationHandleCore* instance = index->second;
    DCHECK(instance);
    return std::move(instance->precreated_host_);
  }
  return std::unique_ptr<AppCacheHost>();
}

AppCacheServiceImpl* AppCacheNavigationHandleCore::GetAppCacheService() {
  return static_cast<AppCacheServiceImpl*>(appcache_service_.get());
}

void AppCacheNavigationHandleCore::SetProcessId(int process_id) {
  DCHECK_EQ(process_id_, ChildProcessHost::kInvalidUniqueID);
  DCHECK_NE(process_id, ChildProcessHost::kInvalidUniqueID);
  DCHECK(precreated_host_);
  process_id_ = process_id;
  precreated_host_->SetProcessId(process_id);
}

}  // namespace content
