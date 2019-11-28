// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_DOWNLOAD_DELEGATE_H_
#define WEBLAYER_PUBLIC_DOWNLOAD_DELEGATE_H_

#include <string>

class GURL;

namespace weblayer {

// An interface that allows clients to handle download requests originating in
// the browser.
class DownloadDelegate {
 public:
  // A download of |url| has been requested with the specified details. If
  // it returns true the download will be considered intercepted and WebLayer
  // won't proceed with it. Note that there are many corner cases where the
  // embedder downloading it won't work (e.g. POSTs, one-time URLs, requests
  // that depend on cookies or auth state). If the method returns false, then
  // currently WebLayer won't download it but in the future this will be hooked
  // up with new callbacks in this interface.
  virtual bool InterceptDownload(const GURL& url,
                                 const std::string& user_agent,
                                 const std::string& content_disposition,
                                 const std::string& mime_type,
                                 int64_t content_length) = 0;

 protected:
  virtual ~DownloadDelegate() {}
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_DOWNLOAD_DELEGATE_H_
