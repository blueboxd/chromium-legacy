// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/web_test/web_test_client_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/content_index_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/browser/shell_content_index_provider.h"
#include "content/shell/browser/web_test/web_test_browser_context.h"
#include "content/shell/browser/web_test/web_test_content_browser_client.h"
#include "content/shell/browser/web_test/web_test_control_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "storage/browser/database/database_tracker.h"
#include "storage/browser/file_system/isolated_context.h"

namespace content {

// static
void WebTestClientImpl::Create(
    int render_process_id,
    mojo::PendingAssociatedReceiver<mojom::WebTestClient> receiver) {
  mojo::MakeSelfOwnedAssociatedReceiver(
      std::make_unique<WebTestClientImpl>(render_process_id),
      std::move(receiver));
}

WebTestClientImpl::WebTestClientImpl(int render_process_id)
    : render_process_id_(render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

WebTestClientImpl::~WebTestClientImpl() = default;

void WebTestClientImpl::WebTestRuntimeFlagsChanged(
    base::Value changed_web_test_runtime_flags) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!WebTestControlHost::Get())
    return;

  base::DictionaryValue* changed_web_test_runtime_flags_dictionary = nullptr;
  bool ok = changed_web_test_runtime_flags.GetAsDictionary(
      &changed_web_test_runtime_flags_dictionary);
  DCHECK(ok);

  WebTestControlHost::Get()->OnWebTestRuntimeFlagsChanged(
      render_process_id_, *changed_web_test_runtime_flags_dictionary);
}

void WebTestClientImpl::RegisterIsolatedFileSystem(
    const std::vector<base::FilePath>& file_paths,
    RegisterIsolatedFileSystemCallback callback) {
  ChildProcessSecurityPolicy* policy =
      ChildProcessSecurityPolicy::GetInstance();

  storage::IsolatedContext::FileInfoSet file_info_set;
  for (auto& path : file_paths) {
    file_info_set.AddPath(path, nullptr);
    if (!policy->CanReadFile(render_process_id_, path))
      policy->GrantReadFile(render_process_id_, path);
  }

  std::string filesystem_id =
      storage::IsolatedContext::GetInstance()->RegisterDraggedFileSystem(
          file_info_set);
  policy->GrantReadFileSystem(render_process_id_, filesystem_id);

  std::move(callback).Run(filesystem_id);
}

}  // namespace content
