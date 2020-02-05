// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/url_loader/request_conversion.h"

#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/request_priority.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/mojom/data_pipe_getter.mojom-blink.h"
#include "services/network/public/mojom/data_pipe_getter.mojom.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-shared.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/network/wrapped_data_pipe_getter.h"

namespace blink {

namespace {

// TODO(yhirano): Unify these with variables in
// content/public/common/content_constants.h.
constexpr char kCorsExemptPurposeHeaderName[] = "Purpose";
constexpr char kCorsExemptRequestedWithHeaderName[] = "X-Requested-With";

// This is complementary to ConvertNetPriorityToWebKitPriority, defined in
// service_worker_context_client.cc.
net::RequestPriority ConvertWebKitPriorityToNetPriority(
    WebURLRequest::Priority priority) {
  switch (priority) {
    case WebURLRequest::Priority::kVeryHigh:
      return net::HIGHEST;

    case WebURLRequest::Priority::kHigh:
      return net::MEDIUM;

    case WebURLRequest::Priority::kMedium:
      return net::LOW;

    case WebURLRequest::Priority::kLow:
      return net::LOWEST;

    case WebURLRequest::Priority::kVeryLow:
      return net::IDLE;

    case WebURLRequest::Priority::kUnresolved:
    default:
      NOTREACHED();
      return net::LOW;
  }
}

// TODO(yhirano) Dedupe this and the same-name function in
// web_url_request_util.cc.
std::string TrimLWSAndCRLF(const base::StringPiece& input) {
  base::StringPiece string = net::HttpUtil::TrimLWS(input);
  const char* begin = string.data();
  const char* end = string.data() + string.size();
  while (begin < end && (end[-1] == '\r' || end[-1] == '\n'))
    --end;
  return std::string(base::StringPiece(begin, end - begin));
}

}  // namespace

void PopulateResourceRequestBody(const EncodedFormData& src,
                                 network::ResourceRequestBody* dest) {
  for (const auto& element : src.Elements()) {
    switch (element.type_) {
      case FormDataElement::kData:
        dest->AppendBytes(element.data_.data(), element.data_.size());
        break;
      case FormDataElement::kEncodedFile:
        if (element.file_length_ == -1) {
          dest->AppendFileRange(
              WebStringToFilePath(element.filename_), 0,
              std::numeric_limits<uint64_t>::max(),
              element.expected_file_modification_time_.value_or(base::Time()));
        } else {
          dest->AppendFileRange(
              WebStringToFilePath(element.filename_),
              static_cast<uint64_t>(element.file_start_),
              static_cast<uint64_t>(element.file_length_),
              element.expected_file_modification_time_.value_or(base::Time()));
        }
        break;
      case FormDataElement::kEncodedBlob: {
        DCHECK(element.optional_blob_data_handle_);
        mojo::Remote<mojom::Blob> blob_remote(mojo::PendingRemote<mojom::Blob>(
            element.optional_blob_data_handle_->CloneBlobRemote().PassPipe(),
            mojom::Blob::Version_));
        mojo::PendingRemote<network::mojom::DataPipeGetter>
            data_pipe_getter_remote;
        blob_remote->AsDataPipeGetter(
            data_pipe_getter_remote.InitWithNewPipeAndPassReceiver());
        dest->AppendDataPipe(std::move(data_pipe_getter_remote));
        break;
      }
      case FormDataElement::kDataPipe: {
        // Convert network::mojom::blink::DataPipeGetter to
        // network::mojom::DataPipeGetter through a raw message pipe.
        mojo::PendingRemote<network::mojom::blink::DataPipeGetter>
            pending_data_pipe_getter;
        element.data_pipe_getter_->GetDataPipeGetter()->Clone(
            pending_data_pipe_getter.InitWithNewPipeAndPassReceiver());
        dest->AppendDataPipe(
            mojo::PendingRemote<network::mojom::DataPipeGetter>(
                pending_data_pipe_getter.PassPipe(), 0u));
        break;
      }
    }
  }
}

void PopulateResourceRequest(const ResourceRequest& src,
                             network::ResourceRequest* dest) {
  dest->method = src.HttpMethod().Latin1();
  dest->url = src.Url();
  dest->site_for_cookies = src.SiteForCookies();
  dest->upgrade_if_insecure = src.UpgradeIfInsecure();
  dest->is_revalidating = src.IsRevalidating();
  if (src.RequestorOrigin()->ToString() == "null") {
    // "file:" origin is treated like an opaque unique origin when
    // allow-file-access-from-files is not specified. Such origin is not
    // opaque (i.e., IsOpaque() returns false) but still serializes to
    // "null".
    dest->request_initiator = url::Origin();
  } else {
    dest->request_initiator = src.RequestorOrigin()->ToUrlOrigin();
  }
  if (src.IsolatedWorldOrigin()) {
    dest->isolated_world_origin = src.IsolatedWorldOrigin()->ToUrlOrigin();
  }
  dest->referrer = WebStringToGURL(src.ReferrerString());

  // "default" referrer policy has already been resolved.
  DCHECK_NE(src.GetReferrerPolicy(), network::mojom::ReferrerPolicy::kDefault);
  dest->referrer_policy =
      network::ReferrerPolicyForUrlRequest(src.GetReferrerPolicy());

  for (const auto& item : src.HttpHeaderFields()) {
    const std::string name = item.key.Latin1();
    const std::string value = TrimLWSAndCRLF(item.value.Latin1());
    dest->headers.SetHeader(name, value);
  }
  // Set X-Requested-With header to cors_exempt_headers rather than headers to
  // be exempted from CORS checks.
  if (!src.GetRequestedWithHeader().IsEmpty()) {
    dest->cors_exempt_headers.SetHeader(kCorsExemptRequestedWithHeaderName,
                                        src.GetRequestedWithHeader().Utf8());
  }
  // Set Purpose header to cors_exempt_headers rather than headers to be
  // exempted from CORS checks.
  if (!src.GetPurposeHeader().IsEmpty()) {
    dest->cors_exempt_headers.SetHeader(kCorsExemptPurposeHeaderName,
                                        src.GetPurposeHeader().Utf8());
  }

  // TODO(yhirano): Remove this WrappedResourceRequest.
  dest->load_flags = WrappedResourceRequest(src).GetLoadFlagsForWebUrlRequest();
  dest->recursive_prefetch_token = src.RecursivePrefetchToken();
  dest->priority = ConvertWebKitPriorityToNetPriority(src.Priority());
  dest->should_reset_appcache = src.ShouldResetAppCache();
  dest->is_external_request = src.IsExternalRequest();
  dest->cors_preflight_policy = src.CorsPreflightPolicy();
  dest->skip_service_worker = src.GetSkipServiceWorker();
  dest->mode = src.GetMode();
  dest->destination = src.GetRequestDestination();
  dest->credentials_mode = src.GetCredentialsMode();
  dest->redirect_mode = src.GetRedirectMode();
  dest->fetch_integrity = src.GetFetchIntegrity().Utf8();
  dest->fetch_request_context_type = static_cast<int>(src.GetRequestContext());

  dest->keepalive = src.GetKeepalive();
  dest->has_user_gesture = src.HasUserGesture();
  dest->enable_load_timing = true;
  dest->enable_upload_progress = src.ReportUploadProgress();
  dest->report_raw_headers = src.ReportRawHeaders();
  // TODO(ryansturm): Remove dest->previews_state once it is no
  // longer used in a network delegate. https://crbug.com/842233
  dest->previews_state = static_cast<int>(src.GetPreviewsState());
  dest->throttling_profile_id = src.GetDevToolsToken();

  if (base::UnguessableToken window_id = src.GetFetchWindowId())
    dest->fetch_window_id = base::make_optional(window_id);

  if (src.GetDevToolsId().has_value()) {
    dest->devtools_request_id = src.GetDevToolsId().value().Ascii();
  }

  if (src.IsSignedExchangePrefetchCacheEnabled()) {
    DCHECK_EQ(src.GetRequestContext(), mojom::RequestContextType::PREFETCH);
    dest->is_signed_exchange_prefetch_cache_enabled = true;
  }

  if (const EncodedFormData* body = src.HttpBody()) {
    DCHECK_NE(dest->method, net::HttpRequestHeaders::kGetMethod);
    DCHECK_NE(dest->method, net::HttpRequestHeaders::kHeadMethod);
    dest->request_body = base::MakeRefCounted<network::ResourceRequestBody>();

    PopulateResourceRequestBody(*body, dest->request_body.get());
  }
}

}  // namespace blink
