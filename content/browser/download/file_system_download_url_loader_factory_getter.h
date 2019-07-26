// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_FILE_SYSTEM_DOWNLOAD_URL_LOADER_FACTORY_GETTER_H_
#define CONTENT_BROWSER_DOWNLOAD_FILE_SYSTEM_DOWNLOAD_URL_LOADER_FACTORY_GETTER_H_

#include <string>

#include "base/memory/scoped_refptr.h"
#include "components/download/public/common/download_url_loader_factory_getter.h"
#include "url/gurl.h"

namespace storage {
class FileSystemContext;
}

namespace content {

class RenderFrameHost;

// Class for retrieving the URLLoaderFactory for a file URL.
class FileSystemDownloadURLLoaderFactoryGetter
    : public download::DownloadURLLoaderFactoryGetter {
 public:
  FileSystemDownloadURLLoaderFactoryGetter(
      const GURL& url,
      RenderFrameHost* rfh,
      scoped_refptr<storage::FileSystemContext> file_system_context,
      const std::string& storage_domain);

  // download::DownloadURLLoaderFactoryGetter implementation.
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;

 protected:
  ~FileSystemDownloadURLLoaderFactoryGetter() override;

 private:
  RenderFrameHost* rfh_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  const std::string storage_domain_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemDownloadURLLoaderFactoryGetter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_FILE_SYSTEM_DOWNLOAD_URL_LOADER_FACTORY_GETTER_H_
