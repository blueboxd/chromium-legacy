// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/file_system_download_url_loader_factory_getter.h"

#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "components/download/public/common/download_task_runner.h"
#include "content/browser/fileapi/file_system_url_loader_factory.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace content {

FileSystemDownloadURLLoaderFactoryGetter::
    FileSystemDownloadURLLoaderFactoryGetter(
        const GURL& url,
        RenderFrameHost* rfh,
        scoped_refptr<storage::FileSystemContext> file_system_context,
        const std::string& storage_domain)
    : rfh_(rfh),
      file_system_context_(file_system_context),
      storage_domain_(storage_domain) {
  DCHECK(url.SchemeIs(url::kFileSystemScheme));
  DCHECK(rfh);
}

FileSystemDownloadURLLoaderFactoryGetter::
    ~FileSystemDownloadURLLoaderFactoryGetter() = default;

scoped_refptr<network::SharedURLLoaderFactory>
FileSystemDownloadURLLoaderFactoryGetter::GetURLLoaderFactory() {
  DCHECK(download::GetIOTaskRunner()->BelongsToCurrentThread());

  std::unique_ptr<network::mojom::URLLoaderFactory> factory =
      CreateFileSystemURLLoaderFactory(rfh_->GetProcess()->GetID(),
                                       rfh_->GetFrameTreeNodeId(),
                                       file_system_context_, storage_domain_);

  network::mojom::URLLoaderFactoryPtrInfo url_loader_factory_ptr_info;
  mojo::MakeStrongBinding(std::move(factory),
                          MakeRequest(&url_loader_factory_ptr_info));

  return base::MakeRefCounted<network::WrapperSharedURLLoaderFactory>(
      std::move(url_loader_factory_ptr_info));
}

}  // namespace content
