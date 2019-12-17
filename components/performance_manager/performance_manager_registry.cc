// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/embedder/performance_manager_registry.h"

#include "base/containers/flat_set.h"
#include "base/sequence_checker.h"
#include "base/stl_util.h"
#include "components/performance_manager/performance_manager_tab_helper.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/render_process_user_data.h"
#include "content/public/browser/render_process_host.h"

namespace performance_manager {

namespace {
PerformanceManagerRegistry* g_instance = nullptr;
}  // namespace

// Not in anonymous namespace to allow friending.
class PerformanceManagerRegistryImpl
    : public PerformanceManagerRegistry,
      public PerformanceManagerTabHelper::DestructionObserver,
      public RenderProcessUserData::DestructionObserver {
 public:
  PerformanceManagerRegistryImpl();
  ~PerformanceManagerRegistryImpl() override;

  PerformanceManagerRegistryImpl(const PerformanceManagerRegistryImpl&) =
      delete;
  void operator=(const PerformanceManagerRegistryImpl&) = delete;

  // PerformanceManagerRegistry:
  void CreatePageNodeForWebContents(
      content::WebContents* web_contents) override;
  void CreateProcessNodeForRenderProcessHost(
      content::RenderProcessHost* render_process_host) override;
  void TearDown() override;

  // PerformanceManagerTabHelper::DestructionObserver:
  void OnPerformanceManagerTabHelperDestroying(
      content::WebContents* web_contents) override;

  // RenderProcessUserData::DestructionObserver:
  void OnRenderProcessUserDataDestroying(
      content::RenderProcessHost* render_process_host) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // Tracks WebContents and RenderProcessHost for which we have created user
  // data. Used to destroy all user data when the registry is destroyed.
  base::flat_set<content::WebContents*> web_contents_;
  base::flat_set<content::RenderProcessHost*> render_process_hosts_;
};

PerformanceManagerRegistryImpl::PerformanceManagerRegistryImpl() {
  DCHECK(!g_instance);
  g_instance = this;

  // The registry should be created after the PerformanceManager.
  DCHECK(PerformanceManager::IsAvailable());
}

PerformanceManagerRegistryImpl::~PerformanceManagerRegistryImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TearDown() should have been invoked to reset |g_instance| and clear
  // |web_contents_| and |render_process_user_data_| prior to destroying the
  // registry.
  DCHECK(!g_instance);
  DCHECK(web_contents_.empty());
  DCHECK(render_process_hosts_.empty());
}

void PerformanceManagerRegistryImpl::CreatePageNodeForWebContents(
    content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto result = web_contents_.insert(web_contents);
  if (result.second) {
    // Create a PerformanceManagerTabHelper if |web_contents| doesn't already
    // have one. Support for multiple calls to CreatePageNodeForWebContents()
    // with the same WebContents is required for Devtools -- see comment in
    // header file.
    PerformanceManagerTabHelper::CreateForWebContents(web_contents);
    PerformanceManagerTabHelper* tab_helper =
        PerformanceManagerTabHelper::FromWebContents(web_contents);
    DCHECK(tab_helper);
    tab_helper->SetDestructionObserver(this);
  }
}

void PerformanceManagerRegistryImpl::CreateProcessNodeForRenderProcessHost(
    content::RenderProcessHost* render_process_host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto result = render_process_hosts_.insert(render_process_host);
  if (result.second) {
    // Create a RenderProcessUserData if |render_process_host| doesn't already
    // have one.
    RenderProcessUserData* user_data =
        RenderProcessUserData::CreateForRenderProcessHost(render_process_host);
    user_data->SetDestructionObserver(this);
  }
}

void PerformanceManagerRegistryImpl::TearDown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;

  // The registry should be torn down before the PerformanceManager.
  DCHECK(PerformanceManager::IsAvailable());

  for (auto* web_contents : web_contents_) {
    PerformanceManagerTabHelper* tab_helper =
        PerformanceManagerTabHelper::FromWebContents(web_contents);
    DCHECK(tab_helper);
    // Clear the destruction observer to avoid a nested notification.
    tab_helper->SetDestructionObserver(nullptr);
    // Destroy the tab helper.
    tab_helper->TearDown();
    web_contents->RemoveUserData(PerformanceManagerTabHelper::UserDataKey());
  }
  web_contents_.clear();

  for (auto* render_process_host : render_process_hosts_) {
    RenderProcessUserData* user_data =
        RenderProcessUserData::GetForRenderProcessHost(render_process_host);
    DCHECK(user_data);
    // Clear the destruction observer to avoid a nested notification.
    user_data->SetDestructionObserver(nullptr);
    // Destroy the user data.
    render_process_host->RemoveUserData(RenderProcessUserData::UserDataKey());
  }
  render_process_hosts_.clear();
}

void PerformanceManagerRegistryImpl::OnPerformanceManagerTabHelperDestroying(
    content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const size_t num_removed = web_contents_.erase(web_contents);
  DCHECK_EQ(1U, num_removed);
}

void PerformanceManagerRegistryImpl::OnRenderProcessUserDataDestroying(
    content::RenderProcessHost* render_process_host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const size_t num_removed = render_process_hosts_.erase(render_process_host);
  DCHECK_EQ(1U, num_removed);
}

// static
std::unique_ptr<PerformanceManagerRegistry>
PerformanceManagerRegistry::Create() {
  return std::make_unique<PerformanceManagerRegistryImpl>();
}

// static
PerformanceManagerRegistry* PerformanceManagerRegistry::GetInstance() {
  return g_instance;
}

}  // namespace performance_manager
