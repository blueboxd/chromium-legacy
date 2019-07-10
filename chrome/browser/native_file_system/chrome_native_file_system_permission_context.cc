// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/native_file_system/chrome_native_file_system_permission_context.h"

#include <utility>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "chrome/browser/permissions/permission_util.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace {

void ShowWritePermissionPromptOnUIThread(
    int process_id,
    int frame_id,
    const url::Origin& origin,
    const base::FilePath& path,
    bool is_directory,
    base::OnceCallback<void(PermissionAction result)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(process_id, frame_id);
  if (!rfh || !rfh->IsCurrent()) {
    // Requested from a no longer valid render frame host.
    std::move(callback).Run(PermissionAction::DISMISSED);
    return;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);

  if (!web_contents) {
    // Requested from a worker, or a no longer existing tab.
    std::move(callback).Run(PermissionAction::DISMISSED);
    return;
  }

  ShowNativeFileSystemPermissionDialog(origin, path, is_directory,
                                       std::move(callback), web_contents);
}

// Returns a callback that calls the passed in |callback| by posting a task to
// the current sequenced task runner.
base::OnceCallback<void(PermissionAction result)>
BindPermissionActionCallbackToCurrentSequence(
    base::OnceCallback<void(PermissionAction result)> callback) {
  return base::BindOnce(
      [](scoped_refptr<base::TaskRunner> task_runner,
         base::OnceCallback<void(PermissionAction result)> callback,
         PermissionAction result) {
        task_runner->PostTask(FROM_HERE,
                              base::BindOnce(std::move(callback), result));
      },
      base::SequencedTaskRunnerHandle::Get(), std::move(callback));
}

}  // namespace

class ChromeNativeFileSystemPermissionContext::PermissionGrantImpl
    : public content::NativeFileSystemPermissionGrant {
 public:
  PermissionGrantImpl(
      scoped_refptr<ChromeNativeFileSystemPermissionContext> context,
      const url::Origin& origin,
      const base::FilePath& path,
      bool is_directory)
      : context_(std::move(context)),
        origin_(origin),
        path_(path),
        is_directory_(is_directory) {}

  const url::Origin& origin() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return origin_;
  }

  const base::FilePath& path() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return path_;
  }

  PermissionStatus GetStatus() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return status_;
  }

  void RequestPermission(int process_id,
                         int frame_id,
                         base::OnceClosure callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (GetStatus() != PermissionStatus::ASK) {
      std::move(callback).Run();
      return;
    }

    auto result_callback = BindPermissionActionCallbackToCurrentSequence(
        base::BindOnce(&PermissionGrantImpl::OnPermissionRequestComplete, this,
                       std::move(callback)));

    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&ShowWritePermissionPromptOnUIThread, process_id,
                       frame_id, origin_, path_, is_directory_,
                       std::move(result_callback)));
  }

 protected:
  ~PermissionGrantImpl() override { context_->PermissionGrantDestroyed(this); }

 private:
  void OnPermissionRequestComplete(base::OnceClosure callback,
                                   PermissionAction result) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    switch (result) {
      case PermissionAction::GRANTED:
        status_ = PermissionStatus::GRANTED;
        break;
      case PermissionAction::DENIED:
        status_ = PermissionStatus::DENIED;
        break;
      case PermissionAction::DISMISSED:
      case PermissionAction::IGNORED:
        break;
      case PermissionAction::REVOKED:
      case PermissionAction::NUM:
        NOTREACHED();
        break;
    }

    std::move(callback).Run();
  }

  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<ChromeNativeFileSystemPermissionContext> const context_;
  const url::Origin origin_;
  const base::FilePath path_;
  const bool is_directory_;
  PermissionStatus status_ = PermissionStatus::ASK;

  DISALLOW_COPY_AND_ASSIGN(PermissionGrantImpl);
};

struct ChromeNativeFileSystemPermissionContext::OriginState {
  // Raw pointers, owned collectively by all the handles that reference this
  // grant. When last reference goes away this state is cleared as well by
  // PermissionGrantDestroyed().
  // TODO(mek): Lifetime of grants might change depending on the outcome of
  // the discussions surrounding lifetime of non-persistent permissions.
  std::map<base::FilePath, PermissionGrantImpl*> grants;
};

ChromeNativeFileSystemPermissionContext::
    ChromeNativeFileSystemPermissionContext(content::BrowserContext*) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

scoped_refptr<content::NativeFileSystemPermissionGrant>
ChromeNativeFileSystemPermissionContext::GetWritePermissionGrant(
    const url::Origin& origin,
    const base::FilePath& path,
    bool is_directory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // operator[] might insert a new OriginState in |origins_|, but that is
  // exactly what we want.
  auto& origin_state = origins_[origin];
  // TODO(mek): Do some kind of clever merging of grants, for example
  // returning a parent directory's grant if that one already has write
  // access.
  auto*& existing_grant = origin_state.grants[path];
  if (existing_grant)
    return existing_grant;
  auto result = base::MakeRefCounted<PermissionGrantImpl>(this, origin, path,
                                                          is_directory);
  existing_grant = result.get();
  return result;
}

ChromeNativeFileSystemPermissionContext::
    ~ChromeNativeFileSystemPermissionContext() = default;

void ChromeNativeFileSystemPermissionContext::ShutdownOnUIThread() {}

void ChromeNativeFileSystemPermissionContext::PermissionGrantDestroyed(
    PermissionGrantImpl* grant) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = origins_.find(grant->origin());
  DCHECK(it != origins_.end());
  it->second.grants.erase(grant->path());
  if (it->second.grants.empty()) {
    // No more grants for the origin, so remove the state.
    origins_.erase(it);
  }
}
