// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/safe_browsing/safe_browsing_service.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/task/post_task.h"
#include "build/branding_buildflags.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/browser/url_checker_delegate.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "components/safe_browsing/core/db/v4_local_database_manager.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - SafeBrowsingService

SafeBrowsingService::SafeBrowsingService() = default;

SafeBrowsingService::~SafeBrowsingService() = default;

void SafeBrowsingService::Initialize(PrefService* prefs,
                                     const base::FilePath& user_data_path) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);

  if (io_thread_enabler_) {
    // Already initialized.
    return;
  }

  base::FilePath safe_browsing_data_path =
      user_data_path.Append(safe_browsing::kSafeBrowsingBaseFilename);
  safe_browsing_db_manager_ = safe_browsing::V4LocalDatabaseManager::Create(
      safe_browsing_data_path, safe_browsing::ExtendedReportingLevelCallback());

  io_thread_enabler_ =
      base::MakeRefCounted<SafeBrowsingService::IOThreadEnabler>(
          safe_browsing_db_manager_);

  base::PostTask(
      FROM_HERE, {web::WebThread::IO},
      base::BindOnce(&SafeBrowsingService::IOThreadEnabler::Initialize,
                     io_thread_enabler_, base::WrapRefCounted(this),
                     network_context_client_.BindNewPipeAndPassReceiver()));

  // Watch for changes to the Safe Browsing opt-out preference.
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(prefs);
  pref_change_registrar_->Add(
      prefs::kSafeBrowsingEnabled,
      base::Bind(&SafeBrowsingService::UpdateSafeBrowsingEnabledState,
                 base::Unretained(this)));
  UpdateSafeBrowsingEnabledState();
}

void SafeBrowsingService::ShutDown() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);

  pref_change_registrar_.reset();
  base::PostTask(FROM_HERE, {web::WebThread::IO},
                 base::BindOnce(&SafeBrowsingService::IOThreadEnabler::ShutDown,
                                io_thread_enabler_));
  network_context_client_.reset();
}

safe_browsing::SafeBrowsingDatabaseManager*
SafeBrowsingService::GetDatabaseManager() {
  return safe_browsing_db_manager_.get();
}

void SafeBrowsingService::SetUpURLLoaderFactory(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  auto url_loader_factory_params =
      network::mojom::URLLoaderFactoryParams::New();
  url_loader_factory_params->process_id = network::mojom::kBrowserProcessId;
  url_loader_factory_params->is_corb_enabled = false;
  network_context_client_->CreateURLLoaderFactory(
      std::move(receiver), std::move(url_loader_factory_params));
}

void SafeBrowsingService::UpdateSafeBrowsingEnabledState() {
  bool enabled =
      pref_change_registrar_->prefs()->GetBoolean(prefs::kSafeBrowsingEnabled);
  base::PostTask(
      FROM_HERE, {web::WebThread::IO},
      base::BindOnce(
          &SafeBrowsingService::IOThreadEnabler::SetSafeBrowsingEnabled,
          io_thread_enabler_, enabled));
}

#pragma mark - SafeBrowsingService::IOThreadEnabler

SafeBrowsingService::IOThreadEnabler::IOThreadEnabler(
    scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager)
    : safe_browsing_db_manager_(database_manager) {}

SafeBrowsingService::IOThreadEnabler::~IOThreadEnabler() = default;

void SafeBrowsingService::IOThreadEnabler::Initialize(
    scoped_refptr<SafeBrowsingService> safe_browsing_service,
    mojo::PendingReceiver<network::mojom::NetworkContext>
        network_context_receiver) {
  SetUpURLRequestContext();
  std::vector<std::string> cors_exempt_header_list;
  network_context_ = std::make_unique<network::NetworkContext>(
      /*network_service=*/nullptr, std::move(network_context_receiver),
      url_request_context_.get(), cors_exempt_header_list);
  SetUpURLLoaderFactory(safe_browsing_service);
}

void SafeBrowsingService::IOThreadEnabler::ShutDown() {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  shutting_down_ = true;
  SetSafeBrowsingEnabled(false);
  url_loader_factory_.reset();
  network_context_.reset();
  shared_url_loader_factory_.reset();
  url_request_context_.reset();
}

void SafeBrowsingService::IOThreadEnabler::SetSafeBrowsingEnabled(
    bool enabled) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  if (enabled_ == enabled)
    return;

  enabled_ = enabled;
  if (enabled_)
    StartSafeBrowsingDBManager();
  else
    safe_browsing_db_manager_->StopOnIOThread(shutting_down_);
}

void SafeBrowsingService::IOThreadEnabler::StartSafeBrowsingDBManager() {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);

  std::string client_name;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  client_name = "googlechrome";
#else
  client_name = "chromium";
#endif

  safe_browsing::V4ProtocolConfig config = safe_browsing::GetV4ProtocolConfig(
      client_name, /*disable_auto_update=*/false);

  safe_browsing_db_manager_->StartOnIOThread(shared_url_loader_factory_,
                                             config);
}

void SafeBrowsingService::IOThreadEnabler::SetUpURLRequestContext() {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);

  // This uses an in-memory non-persistent cookie store. The Safe Browsing V4
  // Update API does not depend on cookies.
  net::URLRequestContextBuilder builder;
  url_request_context_ = builder.Build();
}

void SafeBrowsingService::IOThreadEnabler::SetUpURLLoaderFactory(
    scoped_refptr<SafeBrowsingService> safe_browsing_service) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  base::PostTask(
      FROM_HERE, {web::WebThread::UI},
      base::BindOnce(&SafeBrowsingService::SetUpURLLoaderFactory,
                     safe_browsing_service,
                     url_loader_factory_.BindNewPipeAndPassReceiver()));
  shared_url_loader_factory_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          url_loader_factory_.get());
}
