// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILEAPI_EXTERNAL_FILE_URL_REQUEST_JOB_H_
#define CHROME_BROWSER_CHROMEOS_FILEAPI_EXTERNAL_FILE_URL_REQUEST_JOB_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/fileapi/external_file_resolver.h"
#include "components/drive/file_errors.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_job.h"
#include "url/gurl.h"

namespace net {
class IOBuffer;
class NetworkDelegate;
class URLRequest;
}  // namespace net
namespace storage {
class FileStreamReader;
}

namespace chromeos {

// ExternalFileURLRequestJob is the gateway between network-level drive:...
// requests for drive resources and FileSystem.  It exposes content URLs
// formatted as drive:<drive-file-path>.
// The methods should be run on IO thread.
// TODO(hirono): After removing MHTML support, stop to use the special
// externalfile: scheme and use filesystem: URL directly.  crbug.com/415455
class ExternalFileURLRequestJob : public net::URLRequestJob {
 public:
  ExternalFileURLRequestJob(void* profile_id,
                            net::URLRequest* request,
                            net::NetworkDelegate* network_delegate);

  // net::URLRequestJob overrides:
  void SetExtraRequestHeaders(const net::HttpRequestHeaders& headers) override;
  void Start() override;
  void Kill() override;
  bool GetMimeType(std::string* mime_type) const override;
  bool IsRedirectResponse(GURL* location,
                          int* http_status_code,
                          bool* insecure_scheme_was_upgraded) override;
  int ReadRawData(net::IOBuffer* buf, int buf_size) override;

 protected:
  ~ExternalFileURLRequestJob() override;

 private:
  // Helper method to start the job. Should be called asynchronously because
  // NotifyStartError() is not legal to call synchronously in
  // URLRequestJob::Start().
  void StartAsync();

  void OnRedirectURLObtained(const std::string& mime_type,
                             const GURL& redirect_url);

  void OnStreamObtained(
      const std::string& mime_type,
      storage::IsolatedContext::ScopedFSHandle isolated_file_system_scope,
      std::unique_ptr<storage::FileStreamReader> stream_reader,
      int64_t size);

  void OnError(net::Error error);

  // Called when DriveFileStreamReader::Read is completed.
  void OnReadCompleted(int read_result);

  std::unique_ptr<ExternalFileResolver> resolver_;

  std::string mime_type_;
  storage::IsolatedContext::ScopedFSHandle isolated_file_system_scope_;
  std::unique_ptr<storage::FileStreamReader> stream_reader_;
  int64_t remaining_bytes_;
  GURL redirect_url_;

  // This should remain the last member so it'll be destroyed first and
  // invalidate its weak pointers before other members are destroyed.
  base::WeakPtrFactory<ExternalFileURLRequestJob> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(ExternalFileURLRequestJob);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILEAPI_EXTERNAL_FILE_URL_REQUEST_JOB_H_
