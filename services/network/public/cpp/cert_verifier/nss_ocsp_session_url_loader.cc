// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cert_verifier/nss_ocsp_session_url_loader.h"
#include <type_traits>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "url/gurl.h"

namespace cert_verifier {

namespace {
// The maximum size in bytes for the response body when fetching an OCSP/CRL
// URL.
const int kMaxResponseSizeInBytes = 5 * 1024 * 1024;

bool CanFetchUrl(const GURL& url) {
  return url.SchemeIs("http");
}

}  // namespace

OCSPRequestSessionDelegateURLLoader::OCSPRequestSessionDelegateURLLoader(
    scoped_refptr<base::SequencedTaskRunner> load_task_runner,
    base::WeakPtr<OCSPRequestSessionDelegateFactoryURLLoader> delegate_factory)
    : load_task_runner_(std::move(load_task_runner)),
      delegate_factory_(std::move(delegate_factory)) {}

OCSPRequestSessionDelegateURLLoader::~OCSPRequestSessionDelegateURLLoader() =
    default;

std::unique_ptr<net::OCSPRequestSessionResult>
OCSPRequestSessionDelegateURLLoader::StartAndWait(
    const net::OCSPRequestSessionParams* params) {
  Start(params);
  return WaitForResult();
}

void OCSPRequestSessionDelegateURLLoader::Start(
    const net::OCSPRequestSessionParams* params) {
  load_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&OCSPRequestSessionDelegateURLLoader::StartLoad,
                                this, params));
}

std::unique_ptr<net::OCSPRequestSessionResult>
OCSPRequestSessionDelegateURLLoader::WaitForResult() {
  // Sit and wait for the load to finish.
  wait_event_.Wait();

  return std::move(result_);
}

void OCSPRequestSessionDelegateURLLoader::StartLoad(
    const net::OCSPRequestSessionParams* params) {
  DCHECK(load_task_runner_->RunsTasksInCurrentSequence());

  if (!CanFetchUrl(params->url) || !delegate_factory_) {
    CancelLoad();
    return;
  }

  // Start the SimpleURLLoader.
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("ocsp_start_url_loader", R"(
        semantics {
          sender: "OCSP"
          description:
            "Verifying the revocation status of a certificate via OCSP."
          trigger:
            "This may happen in response to visiting a website that uses "
            "https://"
          data:
            "Identifier for the certificate whose revocation status is being "
            "checked. See https://tools.ietf.org/html/rfc6960#section-2.1 for "
            "more details."
          destination: OTHER
          destination_other:
            "The URI specified in the certificate."
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled by settings."
          policy_exception_justification: "Not implemented."
        })");

  // Create a ResourceRequest based on |params|.
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = params->url;
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  request->load_flags = net::LOAD_DISABLE_CACHE;
  // Disable secure DNS for hostname lookups triggered by certificate network
  // fetches to prevent deadlock.
  request->trusted_params = network::ResourceRequest::TrustedParams();
  request->trusted_params->disable_secure_dns = true;

  if (!params->extra_request_headers.IsEmpty())
    request->headers = params->extra_request_headers;

  std::string upload_string;
  if (params->http_request_method == "POST") {
    DCHECK(!params->upload_content.empty());
    DCHECK(!params->upload_content_type.empty());

    request->method = "POST";
    upload_string = std::string(params->upload_content.data(),
                                params->upload_content.size());
  }

  url_loader_ =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);
  if (!upload_string.empty())
    url_loader_->AttachStringForUpload(std::move(upload_string),
                                       params->upload_content_type);

  url_loader_->SetTimeoutDuration(params->timeout);
  // base::Unretained(this) is safe because |this| owns |url_loader_|, which
  // will not call the callback if it is deleted.
  url_loader_->SetOnRedirectCallback(base::BindRepeating(
      &OCSPRequestSessionDelegateURLLoader::OnReceivedRedirect,
      base::Unretained(this)));
  // The completion callback holds a reference to this class to make sure we
  // always call FinishLoad() and signal the worker thread to continue.
  // We will not touch |this| after deleting |url_loader_|, which may hold the
  // last reference to |this|.
  url_loader_->DownloadToString(
      delegate_factory_->GetSharedURLLoaderFactory().get(),
      base::BindOnce(&OCSPRequestSessionDelegateURLLoader::OnUrlLoaderCompleted,
                     this),
      kMaxResponseSizeInBytes);

  result_ = std::make_unique<net::OCSPRequestSessionResult>();
}

void OCSPRequestSessionDelegateURLLoader::OnReceivedRedirect(
    const net::RedirectInfo& redirect_info,
    const network::mojom::URLResponseHead& response_head,
    std::vector<std::string>* removed_headers) {
  DCHECK(load_task_runner_->RunsTasksInCurrentSequence());

  if (!CanFetchUrl(redirect_info.new_url)) {
    CancelLoad();  // May delete |this|
  }
}

void OCSPRequestSessionDelegateURLLoader::OnUrlLoaderCompleted(
    std::unique_ptr<std::string> response_body) {
  DCHECK(load_task_runner_->RunsTasksInCurrentSequence());

  if (!response_body || !url_loader_->ResponseInfo()) {
    CancelLoad();  // May delete |this|
    return;
  }

  result_->response_code =
      url_loader_->ResponseInfo()->headers->response_code();
  result_->response_headers = url_loader_->ResponseInfo()->headers;
  result_->response_content_type = url_loader_->ResponseInfo()->mime_type;
  result_->data = std::move(*response_body);
  FinishLoad();  // May delete |this|
}

void OCSPRequestSessionDelegateURLLoader::CancelLoad() {
  DCHECK(load_task_runner_->RunsTasksInCurrentSequence());

  result_.reset();
  FinishLoad();  // May delete |this|
}

void OCSPRequestSessionDelegateURLLoader::FinishLoad() {
  DCHECK(load_task_runner_->RunsTasksInCurrentSequence());

  wait_event_.Signal();

  url_loader_.reset();
  // Cannot touch |this| as |url_loader_| may have owned the last reference to
  // |this|.
}

OCSPRequestSessionDelegateFactoryURLLoader::
    OCSPRequestSessionDelegateFactoryURLLoader(
        scoped_refptr<base::SequencedTaskRunner> loader_factory_sequence,
        scoped_refptr<network::SharedURLLoaderFactory> loader_factory)
    : loader_factory_sequence_(std::move(loader_factory_sequence)),
      loader_factory_(std::move(loader_factory)),
      weak_factory_(this) {
  DCHECK(loader_factory_sequence_->RunsTasksInCurrentSequence());

  weak_ptr_ = weak_factory_.GetWeakPtr();
}

scoped_refptr<net::OCSPRequestSessionDelegate>
OCSPRequestSessionDelegateFactoryURLLoader::CreateOCSPRequestSessionDelegate() {
  return base::MakeRefCounted<OCSPRequestSessionDelegateURLLoader>(
      loader_factory_sequence_, weak_ptr_);
}

OCSPRequestSessionDelegateFactoryURLLoader::
    ~OCSPRequestSessionDelegateFactoryURLLoader() {
  DCHECK(loader_factory_sequence_->RunsTasksInCurrentSequence());
}

}  // namespace cert_verifier
