// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/service_worker_loader_helpers.h"

#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/stringprintf.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/http/http_util.h"
#include "net/http/structured_headers.h"
#include "net/url_request/redirect_util.h"
#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
#include "services/network/public/cpp/cross_origin_embedder_policy.h"
#include "services/network/public/cpp/cross_origin_opener_policy_parser.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "ui/base/page_transition_types.h"

namespace content {
namespace {

// Calls |callback| when Blob reading is complete.
class BlobCompleteCaller : public blink::mojom::BlobReaderClient {
 public:
  using BlobCompleteCallback = base::OnceCallback<void(int net_error)>;

  explicit BlobCompleteCaller(BlobCompleteCallback callback)
      : callback_(std::move(callback)) {}
  ~BlobCompleteCaller() override = default;

  void OnCalculatedSize(uint64_t total_size,
                        uint64_t expected_content_size) override {}
  void OnComplete(int32_t status, uint64_t data_length) override {
    std::move(callback_).Run(base::checked_cast<int>(status));
  }

 private:
  BlobCompleteCallback callback_;
};

std::pair<network::mojom::CrossOriginEmbedderPolicyValue,
          base::Optional<std::string>>
ParseCrossOriginEmbedderPolicyValueInternal(
    const net::HttpResponseHeaders* headers,
    base::StringPiece header_name) {
  static constexpr char kRequireCorp[] = "require-corp";
  constexpr auto kNone = network::mojom::CrossOriginEmbedderPolicyValue::kNone;
  using Item = net::structured_headers::Item;
  std::string header_value;
  if (!headers ||
      !headers->GetNormalizedHeader(header_name.as_string(), &header_value)) {
    return std::make_pair(kNone, base::nullopt);
  }
  const auto item = net::structured_headers::ParseItem(header_value);
  if (!item || item->item.Type() != Item::kTokenType ||
      item->item.GetString() != kRequireCorp) {
    return std::make_pair(kNone, base::nullopt);
  }
  base::Optional<std::string> endpoint;
  auto it = std::find_if(item->params.cbegin(), item->params.cend(),
                         [](const std::pair<std::string, Item>& param) {
                           return param.first == "report-to";
                         });
  if (it != item->params.end() && it->second.Type() == Item::kStringType) {
    endpoint = it->second.GetString();
  }
  return std::make_pair(
      network::mojom::CrossOriginEmbedderPolicyValue::kRequireCorp,
      std::move(endpoint));
}

}  // namespace

// static
void ServiceWorkerLoaderHelpers::SaveResponseHeaders(
    const int status_code,
    const std::string& status_text,
    const base::flat_map<std::string, std::string>& headers,
    network::mojom::URLResponseHead* out_head) {
  // Build a string instead of using HttpResponseHeaders::AddHeader on
  // each header, since AddHeader has O(n^2) performance.
  std::string buf(base::StringPrintf("HTTP/1.1 %d %s\r\n", status_code,
                                     status_text.c_str()));
  for (const auto& item : headers) {
    buf.append(item.first);
    buf.append(": ");
    buf.append(item.second);
    buf.append("\r\n");
  }
  buf.append("\r\n");

  out_head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(buf));

  // Populate |out_head|'s MIME type with the value from the HTTP response
  // headers.
  if (out_head->mime_type.empty()) {
    std::string mime_type;
    if (out_head->headers->GetMimeType(&mime_type))
      out_head->mime_type = mime_type;
  }

  // Populate |out_head|'s charset with the value from the HTTP response
  // headers.
  if (out_head->charset.empty()) {
    std::string charset;
    if (out_head->headers->GetCharset(&charset))
      out_head->charset = charset;
  }

  // Populate |out_head|'s content length with the value from the HTTP response
  // headers.
  if (out_head->content_length == -1)
    out_head->content_length = out_head->headers->GetContentLength();

  // TODO(yhirano): Remove the code duplication with
  // //services/network/url_loader.cc.
  if (base::FeatureList::IsEnabled(
          network::features::kCrossOriginEmbedderPolicy)) {
    // Parse the Cross-Origin-Embedder-Policy and
    // Cross-Origin-Embedder-Policy-Report-Only headers.

    static constexpr char kCrossOriginEmbedderPolicyValueHeader[] =
        "Cross-Origin-Embedder-Policy";
    static constexpr char kCrossOriginEmbedderPolicyValueReportOnlyHeader[] =
        "Cross-Origin-Embedder-Policy-Report-Only";
    network::CrossOriginEmbedderPolicy coep;
    std::tie(coep.value, coep.reporting_endpoint) =
        ParseCrossOriginEmbedderPolicyValueInternal(
            out_head->headers.get(), kCrossOriginEmbedderPolicyValueHeader);
    std::tie(coep.report_only_value, coep.report_only_reporting_endpoint) =
        ParseCrossOriginEmbedderPolicyValueInternal(
            out_head->headers.get(),
            kCrossOriginEmbedderPolicyValueReportOnlyHeader);
    out_head->cross_origin_embedder_policy = coep;
  }

  // TODO(pmeuleman): Remove the code duplication with
  // //services/network/url_loader.cc.
  if (base::FeatureList::IsEnabled(
          network::features::kCrossOriginOpenerPolicy)) {
    // Parse the Cross-Origin-Opener-Policy header.
    constexpr char kCrossOriginOpenerPolicyHeader[] =
        "Cross-Origin-Opener-Policy";
    std::string raw_coop_string;
    if (out_head->headers &&
        out_head->headers->GetNormalizedHeader(kCrossOriginOpenerPolicyHeader,
                                               &raw_coop_string)) {
      out_head->cross_origin_opener_policy =
          network::ParseCrossOriginOpenerPolicyHeader(raw_coop_string);
    }
  }
}

// static
void ServiceWorkerLoaderHelpers::SaveResponseInfo(
    const blink::mojom::FetchAPIResponse& response,
    network::mojom::URLResponseHead* out_head) {
  out_head->was_fetched_via_service_worker = true;
  out_head->was_fallback_required_by_service_worker = false;
  out_head->url_list_via_service_worker = response.url_list;
  out_head->response_type = response.response_type;
  out_head->response_time = response.response_time;
  out_head->is_in_cache_storage =
      response.response_source ==
      network::mojom::FetchResponseSource::kCacheStorage;
  if (response.cache_storage_cache_name)
    out_head->cache_storage_cache_name = *(response.cache_storage_cache_name);
  else
    out_head->cache_storage_cache_name.clear();
  out_head->cors_exposed_header_names = response.cors_exposed_header_names;
  out_head->did_service_worker_navigation_preload = false;
  out_head->content_security_policy =
      mojo::Clone(response.content_security_policy);
}

// static
base::Optional<net::RedirectInfo>
ServiceWorkerLoaderHelpers::ComputeRedirectInfo(
    const network::ResourceRequest& original_request,
    const network::mojom::URLResponseHead& response_head) {
  std::string new_location;
  if (!response_head.headers->IsRedirect(&new_location))
    return base::nullopt;

  // If the request is a MAIN_FRAME request, the first-party URL gets
  // updated on redirects.
  const net::URLRequest::FirstPartyURLPolicy first_party_url_policy =
      original_request.resource_type ==
              static_cast<int>(blink::mojom::ResourceType::kMainFrame)
          ? net::URLRequest::UPDATE_FIRST_PARTY_URL_ON_REDIRECT
          : net::URLRequest::NEVER_CHANGE_FIRST_PARTY_URL;
  return net::RedirectInfo::ComputeRedirectInfo(
      original_request.method, original_request.url,
      original_request.site_for_cookies, first_party_url_policy,
      original_request.referrer_policy,
      original_request.referrer.GetAsReferrer().spec(),
      response_head.headers->response_code(),
      original_request.url.Resolve(new_location),
      net::RedirectUtil::GetReferrerPolicyHeader(response_head.headers.get()),
      false /* insecure_scheme_was_upgraded */);
}

int ServiceWorkerLoaderHelpers::ReadBlobResponseBody(
    mojo::Remote<blink::mojom::Blob>* blob,
    uint64_t blob_size,
    base::OnceCallback<void(int)> on_blob_read_complete,
    mojo::ScopedDataPipeConsumerHandle* handle_out) {
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = blink::BlobUtils::GetDataPipeCapacity(blob_size);

  mojo::ScopedDataPipeProducerHandle producer_handle;
  MojoResult rv = mojo::CreateDataPipe(&options, &producer_handle, handle_out);
  if (rv != MOJO_RESULT_OK)
    return net::ERR_FAILED;

  mojo::PendingRemote<blink::mojom::BlobReaderClient> blob_reader_client;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<BlobCompleteCaller>(std::move(on_blob_read_complete)),
      blob_reader_client.InitWithNewPipeAndPassReceiver());

  (*blob)->ReadAll(std::move(producer_handle), std::move(blob_reader_client));
  return net::OK;
}

}  // namespace content
