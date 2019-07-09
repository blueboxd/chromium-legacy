// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_impl_io_data.h"

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/time/default_clock.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_io_data.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/net/chrome_network_delegate.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/cookie_config/cookie_store_util.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/core/browser/data_store_impl.h"
#include "components/net_log/chrome_net_log.h"
#include "components/network_session_configurator/browser/network_session_configurator.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_filter.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cookie_store_factory.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/extension_protocols.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/constants.h"
#include "net/base/cache_type.h"
#include "net/cookies/cookie_store.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_server_properties_manager.h"
#include "net/net_buildflags.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_intercepting_job_factory.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "storage/browser/quota/special_storage_policy.h"

using content::BrowserThread;

ProfileImplIOData::Handle::Handle(Profile* profile)
    : io_data_(new ProfileImplIOData),
      profile_(profile),
      initialized_(false) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(profile);
}

ProfileImplIOData::Handle::~Handle() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // io_data_->data_reduction_proxy_io_data() might be NULL if Init() was
  // never called.
  if (io_data_->data_reduction_proxy_io_data())
    io_data_->data_reduction_proxy_io_data()->ShutdownOnUIThread();

  io_data_->ShutdownOnUIThread();
}

void ProfileImplIOData::Handle::Init(const base::FilePath& profile_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Keep track of profile path for data reduction proxy..
  io_data_->profile_path_ = profile_path;

  io_data_->set_data_reduction_proxy_io_data(
      CreateDataReductionProxyChromeIOData(
          profile_,
          base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}),
          base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::UI})));

#if defined(OS_CHROMEOS)
  io_data_->data_reduction_proxy_io_data()
      ->config()
      ->EnableGetNetworkIdAsynchronously();
#endif
}

content::ResourceContext*
    ProfileImplIOData::Handle::GetResourceContext() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  LazyInitialize();
  return GetResourceContextNoInit();
}

content::ResourceContext*
ProfileImplIOData::Handle::GetResourceContextNoInit() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Don't call LazyInitialize here, since the resource context is created at
  // the beginning of initalization and is used by some members while they're
  // being initialized (i.e. AppCacheService).
  return io_data_->GetResourceContext();
}

void ProfileImplIOData::Handle::InitializeDataReductionProxy() const {
  scoped_refptr<base::SequencedTaskRunner> db_task_runner =
      base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  std::unique_ptr<data_reduction_proxy::DataStore> store(
      new data_reduction_proxy::DataStoreImpl(io_data_->profile_path_));
  DataReductionProxyChromeSettingsFactory::GetForBrowserContext(profile_)
      ->InitDataReductionProxySettings(
          io_data_->data_reduction_proxy_io_data(), profile_->GetPrefs(),
          profile_,
          content::BrowserContext::GetDefaultStoragePartition(profile_)
              ->GetURLLoaderFactoryForBrowserProcess(),
          std::move(store),
          base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::UI}),
          db_task_runner);
}

void ProfileImplIOData::Handle::LazyInitialize() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (initialized_)
    return;

  // Set initialized_ to true at the beginning in case any of the objects
  // below try to get the ResourceContext pointer.
  initialized_ = true;
  PrefService* pref_service = profile_->GetPrefs();
  io_data_->safe_browsing_enabled()->Init(prefs::kSafeBrowsingEnabled,
      pref_service);
  io_data_->safe_browsing_enabled()->MoveToSequence(
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}));
  io_data_->safe_browsing_whitelist_domains()->Init(
      prefs::kSafeBrowsingWhitelistDomains, pref_service);
  io_data_->safe_browsing_whitelist_domains()->MoveToSequence(
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}));
#if BUILDFLAG(ENABLE_PLUGINS)
  io_data_->always_open_pdf_externally()->Init(
      prefs::kPluginsAlwaysOpenPdfExternally, pref_service);
  io_data_->always_open_pdf_externally()->MoveToSequence(
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}));
#endif
  io_data_->InitializeOnUIThread(profile_);
}

ProfileImplIOData::ProfileImplIOData()
    : ProfileIOData(Profile::REGULAR_PROFILE) {}

ProfileImplIOData::~ProfileImplIOData() {
  DestroyResourceContext();
}
