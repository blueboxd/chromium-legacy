// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/fileapi/external_file_url_request_job.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_status.h"
#include "storage/browser/fileapi/file_stream_reader.h"

using content::BrowserThread;

namespace chromeos {

ExternalFileURLRequestJob::ExternalFileURLRequestJob(
    void* profile_id,
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate)
    : net::URLRequestJob(request, network_delegate),
      resolver_(std::make_unique<ExternalFileResolver>(profile_id)) {}

void ExternalFileURLRequestJob::SetExtraRequestHeaders(
    const net::HttpRequestHeaders& headers) {
  resolver_->ProcessHeaders(headers);
}

void ExternalFileURLRequestJob::Start() {
  // Post a task to invoke StartAsync asynchronously to avoid re-entering the
  // delegate, because NotifyStartError is not legal to call synchronously in
  // Start().
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&ExternalFileURLRequestJob::StartAsync,
                                weak_ptr_factory_.GetWeakPtr()));
}

void ExternalFileURLRequestJob::StartAsync() {
  DVLOG(1) << "Starting request";
  DCHECK(!stream_reader_);
  resolver_->Resolve(
      request()->method(), request()->url(),
      base::BindOnce(&ExternalFileURLRequestJob::OnError,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&ExternalFileURLRequestJob::OnRedirectURLObtained,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&ExternalFileURLRequestJob::OnStreamObtained,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ExternalFileURLRequestJob::OnRedirectURLObtained(
    const std::string& mime_type,
    const GURL& redirect_url) {
  mime_type_ = mime_type;
  redirect_url_ = redirect_url;
  NotifyHeadersComplete();
}

void ExternalFileURLRequestJob::OnStreamObtained(
    const std::string& mime_type,
    storage::IsolatedContext::ScopedFSHandle isolated_file_system_scope,
    std::unique_ptr<storage::FileStreamReader> stream_reader,
    int64_t size) {
  mime_type_ = mime_type;
  isolated_file_system_scope_ = std::move(isolated_file_system_scope);
  stream_reader_ = std::move(stream_reader);
  remaining_bytes_ = size;

  set_expected_content_size(remaining_bytes_);
  NotifyHeadersComplete();
}

void ExternalFileURLRequestJob::OnError(net::Error error) {
  NotifyStartError(net::URLRequestStatus(net::URLRequestStatus::FAILED, error));
}

void ExternalFileURLRequestJob::Kill() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  resolver_.reset();
  stream_reader_.reset();
  isolated_file_system_scope_ = {};
  net::URLRequestJob::Kill();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

bool ExternalFileURLRequestJob::GetMimeType(std::string* mime_type) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  mime_type->assign(mime_type_);
  return !mime_type->empty();
}

bool ExternalFileURLRequestJob::IsRedirectResponse(
    GURL* location,
    int* http_status_code,
    bool* insecure_scheme_was_upgraded) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (redirect_url_.is_empty())
    return false;

  // Redirect a hosted document.
  *insecure_scheme_was_upgraded = false;
  *location = redirect_url_;
  const int kHttpFound = 302;
  *http_status_code = kHttpFound;
  return true;
}

int ExternalFileURLRequestJob::ReadRawData(net::IOBuffer* buf, int buf_size) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(stream_reader_);

  if (remaining_bytes_ == 0)
    return 0;

  const int result = stream_reader_->Read(
      buf, std::min<int64_t>(buf_size, remaining_bytes_),
      base::Bind(&ExternalFileURLRequestJob::OnReadCompleted,
                 weak_ptr_factory_.GetWeakPtr()));

  if (result < 0)
    return result;

  remaining_bytes_ -= result;
  return result;
}

ExternalFileURLRequestJob::~ExternalFileURLRequestJob() {
}

void ExternalFileURLRequestJob::OnReadCompleted(int read_result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (read_result > 0)
    remaining_bytes_ -= read_result;

  ReadRawDataComplete(read_result);
}

}  // namespace chromeos
