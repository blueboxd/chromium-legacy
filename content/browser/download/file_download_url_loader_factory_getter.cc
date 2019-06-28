// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/file_download_url_loader_factory_getter.h"

#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "components/download/public/common/download_task_runner.h"
#include "content/browser/file_url_loader_factory.h"
#include "content/browser/url_loader_factory_getter.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace content {

FileDownloadURLLoaderFactoryGetter::FileDownloadURLLoaderFactoryGetter(
    const GURL& url,
    const base::FilePath& profile_path,
    scoped_refptr<SharedCorsOriginAccessList> shared_cors_origin_access_list)
    : url_(url),
      profile_path_(profile_path),
      shared_cors_origin_access_list_(
          std::move(shared_cors_origin_access_list)) {
  DCHECK(url.SchemeIs(url::kFileScheme));
}

FileDownloadURLLoaderFactoryGetter::~FileDownloadURLLoaderFactoryGetter() =
    default;

scoped_refptr<network::SharedURLLoaderFactory>
FileDownloadURLLoaderFactoryGetter::GetURLLoaderFactory() {
  DCHECK(download::GetIOTaskRunner()->BelongsToCurrentThread());

  network::mojom::URLLoaderFactoryPtrInfo url_loader_factory_ptr_info;
  mojo::MakeStrongBinding(std::make_unique<FileURLLoaderFactory>(
                              profile_path_, shared_cors_origin_access_list_,
                              // USER_VISIBLE because download should progress
                              // even when there is high priority work to do.
                              base::TaskPriority::USER_VISIBLE),
                          MakeRequest(&url_loader_factory_ptr_info));

  return base::MakeRefCounted<network::WrapperSharedURLLoaderFactory>(
      std::move(url_loader_factory_ptr_info));
}

}  // namespace content
