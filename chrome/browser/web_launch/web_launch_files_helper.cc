// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_launch/web_launch_files_helper.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_directory_handle.mojom.h"
#include "url/origin.h"

namespace {

// Converts |paths| to |NativeFileSystemEntries|, which can be manipulated by
// |context|. Must be called on IO thread.
std::vector<blink::mojom::NativeFileSystemEntryPtr> GetEntries(
    scoped_refptr<content::NativeFileSystemEntryFactory> entry_factory,
    content::NativeFileSystemEntryFactory::BindingContext context,
    std::vector<base::FilePath> paths) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(entry_factory);

  std::vector<blink::mojom::NativeFileSystemEntryPtr> launch_entries;
  launch_entries.reserve(paths.size());

  for (const auto& path : paths) {
    launch_entries.push_back(
        entry_factory->CreateFileEntryFromPath(context, path));
  }

  return launch_entries;
}

}  // namespace

namespace web_launch {

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebLaunchFilesHelper)

// static
void WebLaunchFilesHelper::SetLaunchPaths(
    content::WebContents* web_contents,
    const GURL& launch_url,
    std::vector<base::FilePath> launch_paths) {
  if (launch_paths.size() == 0)
    return;

  web_contents->SetUserData(
      UserDataKey(), std::make_unique<WebLaunchFilesHelper>(
                         web_contents, launch_url, std::move(launch_paths)));
}

void WebLaunchFilesHelper::DidFinishNavigation(
    content::NavigationHandle* handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Currently, launch data is only sent for the main frame.
  if (!handle->IsInMainFrame())
    return;

  MaybeSendLaunchEntries();
}

WebLaunchFilesHelper::WebLaunchFilesHelper(
    content::WebContents* web_contents,
    const GURL& launch_url,
    std::vector<base::FilePath> launch_paths)
    : content::WebContentsObserver(web_contents), launch_url_(launch_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(launch_paths.size());

  scoped_refptr<content::NativeFileSystemEntryFactory> entry_factory =
      web_contents->GetMainFrame()
          ->GetProcess()
          ->GetStoragePartition()
          ->GetNativeFileSystemEntryFactory();

  content::NativeFileSystemEntryFactory::BindingContext context(
      url::Origin::Create(launch_url),
      web_contents->GetMainFrame()->GetProcess()->GetID(),
      web_contents->GetMainFrame()->GetRoutingID());

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&GetEntries, std::move(entry_factory), std::move(context),
                     std::move(launch_paths)),
      base::BindOnce(&WebLaunchFilesHelper::SetLaunchEntries,
                     weak_ptr_factory.GetWeakPtr()));
}
WebLaunchFilesHelper::~WebLaunchFilesHelper() = default;

void WebLaunchFilesHelper::SetLaunchEntries(
    std::vector<blink::mojom::NativeFileSystemEntryPtr> launch_entries) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  launch_entries_ = std::move(launch_entries);
  MaybeSendLaunchEntries();
}

void WebLaunchFilesHelper::MaybeSendLaunchEntries() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (launch_entries_.size() == 0)
    return;
  if (launch_url_ != web_contents()->GetLastCommittedURL())
    return;

  blink::mojom::WebLaunchServiceAssociatedPtr launch_service;
  web_contents()->GetMainFrame()->GetRemoteAssociatedInterfaces()->GetInterface(
      &launch_service);
  DCHECK(launch_service);
  launch_service->SetLaunchFiles(std::move(launch_entries_));

  // LaunchParams are sent, clean up.
  web_contents()->RemoveUserData(UserDataKey());
}

}  // namespace web_launch
