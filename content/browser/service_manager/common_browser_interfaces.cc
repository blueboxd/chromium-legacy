// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_manager/common_browser_interfaces.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/task/post_task.h"
#include "base/task_runner.h"
#include "build/build_config.h"
#include "components/discardable_memory/service/discardable_shared_memory_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/connection_filter.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/service_manager/public/cpp/binder_registry.h"

#if defined(OS_WIN)
#include "content/browser/renderer_host/dwrite_font_proxy_impl_win.h"
#include "content/public/common/font_cache_dispatcher_win.h"
#elif defined(OS_MACOSX)
#include "content/browser/sandbox_support_mac_impl.h"
#endif

namespace content {

namespace {

class ConnectionFilterImpl : public ConnectionFilter {
 public:
  ConnectionFilterImpl() {
#if defined(OS_WIN)
    registry_.AddInterface(base::BindRepeating(&FontCacheDispatcher::Create));
    registry_.AddInterface(
        base::BindRepeating(&DWriteFontProxyImpl::Create),
        base::CreateSequencedTaskRunner({base::ThreadPool(),
                                         base::TaskPriority::USER_BLOCKING,
                                         base::MayBlock()}));
#elif defined(OS_MACOSX)
    registry_.AddInterface(
        base::BindRepeating(&SandboxSupportMacImpl::BindRequest,
                            base::Owned(new SandboxSupportMacImpl)));
#endif

    auto* discardable_shared_memory_manager =
        discardable_memory::DiscardableSharedMemoryManager::Get();
    if (discardable_shared_memory_manager) {
      registry_.AddInterface(base::BindRepeating(
          &discardable_memory::DiscardableSharedMemoryManager::Bind,
          base::Unretained(discardable_shared_memory_manager)));
    }
  }

  ~ConnectionFilterImpl() override { DCHECK_CURRENTLY_ON(BrowserThread::IO); }

 private:
  template <typename Interface>
  using InterfaceBinder =
      base::Callback<void(mojo::InterfaceRequest<Interface>,
                          const service_manager::BindSourceInfo&)>;

  // ConnectionFilter:
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle* interface_pipe,
                       service_manager::Connector* connector) override {
    registry_.TryBindInterface(interface_name, interface_pipe, source_info);
  }

  service_manager::BinderRegistryWithArgs<
      const service_manager::BindSourceInfo&>
      registry_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionFilterImpl);
};

}  // namespace

void RegisterCommonBrowserInterfaces(ServiceManagerConnection* connection) {
  connection->AddConnectionFilter(std::make_unique<ConnectionFilterImpl>());
}

}  // namespace content
