// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_browser_process.h"

#include "android_webview/browser/aw_browser_context.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace android_webview {

namespace {
AwBrowserProcess* g_aw_browser_process = nullptr;
}  // namespace

// static
AwBrowserProcess* AwBrowserProcess::GetInstance() {
  return g_aw_browser_process;
}

AwBrowserProcess::AwBrowserProcess(
    AwFeatureListCreator* aw_feature_list_creator) {
  g_aw_browser_process = this;
  aw_feature_list_creator_ = aw_feature_list_creator;
}

AwBrowserProcess::~AwBrowserProcess() {
  g_aw_browser_process = nullptr;
}

PrefService* AwBrowserProcess::local_state() {
  if (!local_state_)
    CreateLocalState();
  return local_state_.get();
}

void AwBrowserProcess::CreateLocalState() {
  DCHECK(!local_state_);

  local_state_ = aw_feature_list_creator_->TakePrefService();
  DCHECK(local_state_);
}

void AwBrowserProcess::InitSafeBrowsing() {
  CreateSafeBrowsingUIManager();
  CreateSafeBrowsingWhitelistManager();
}

void AwBrowserProcess::CreateSafeBrowsingUIManager() {
  safe_browsing_ui_manager_ = new AwSafeBrowsingUIManager(
      AwBrowserContext::GetDefault()->GetAwURLRequestContext());
}

void AwBrowserProcess::CreateSafeBrowsingWhitelistManager() {
  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner =
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO});
  safe_browsing_whitelist_manager_ =
      std::make_unique<AwSafeBrowsingWhitelistManager>(background_task_runner,
                                                       io_task_runner);
}

safe_browsing::RemoteSafeBrowsingDatabaseManager*
AwBrowserProcess::GetSafeBrowsingDBManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!safe_browsing_db_manager_) {
    safe_browsing_db_manager_ =
        new safe_browsing::RemoteSafeBrowsingDatabaseManager();
  }

  if (!safe_browsing_db_manager_started_) {
    // V4ProtocolConfig is not used. Just create one with empty values..
    safe_browsing::V4ProtocolConfig config("", false, "", "");
    safe_browsing_db_manager_->StartOnIOThread(
        GetSafeBrowsingUIManager()->GetURLLoaderFactoryOnIOThread(), config);
    safe_browsing_db_manager_started_ = true;
  }

  return safe_browsing_db_manager_.get();
}

safe_browsing::TriggerManager*
AwBrowserProcess::GetSafeBrowsingTriggerManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!safe_browsing_trigger_manager_) {
    safe_browsing_trigger_manager_ =
        std::make_unique<safe_browsing::TriggerManager>(
            GetSafeBrowsingUIManager(),
            /*referrer_chain_provider=*/nullptr,
            /*local_state_prefs=*/nullptr);
  }

  return safe_browsing_trigger_manager_.get();
}

AwSafeBrowsingWhitelistManager*
AwBrowserProcess::GetSafeBrowsingWhitelistManager() const {
  return safe_browsing_whitelist_manager_.get();
}

AwSafeBrowsingUIManager* AwBrowserProcess::GetSafeBrowsingUIManager() const {
  return safe_browsing_ui_manager_.get();
}

}  // namespace android_webview
