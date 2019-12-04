// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/search_result_loader.h"

#include <utility>

#include "base/json/json_writer.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "net/base/escape.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace chromeos {
namespace quick_answers {
namespace {

using base::Value;
using network::ResourceRequest;
using network::mojom::URLLoaderFactory;

// TODO(llin): Update the policy detail after finalizing on the consent check.
constexpr net::NetworkTrafficAnnotationTag kNetworkTrafficAnnotationTag =
    net::DefineNetworkTrafficAnnotation("quick_answers_loader", R"(
          semantics: {
            sender: "ChromeOS Quick Answers"
            description:
              "ChromeOS requests quick answers based on the currently selected "
              "text."
            trigger:
              "Right click to trigger context menu."
            destination: GOOGLE_OWNED_SERVICE
          }
          policy: {
            cookies_allowed: YES
            setting:
              "Quick Answers can be enabled/disabled in Chrome Settings and is "
              "subject to eligibility requirements. The user may also "
              "separately opt out of sharing screen context with Assistant."
          })");

constexpr base::StringPiece kSearchEndpoint =
    "https://www.google.com/httpservice/web/KnowledgeApiService/Search?"
    "fmt=json";

constexpr char kPayloadParam[] = "reqpld";

// The JSON we generate looks like this:
// {
//   "query": {
//     "raw_query": "23 cm"
//   }
//   "client_id": {
//     "client_type": "EXPERIMENTAL"
//   }
// }
//
// Which is:
// DICT
//   "query": DICT
//      "raw_query": STRING
//   "client_id": DICT
//       "client_type": STRING

constexpr base::StringPiece kQueryKey = "query";
constexpr base::StringPiece kRawQueryKey = "rawQuery";
constexpr base::StringPiece kClientTypeKey = "clientType";
constexpr base::StringPiece kClientIdKey = "clientId";
// TODO(llin): Update the client type after finalize on the client type.
constexpr base::StringPiece kClientType = "EXPERIMENTAL";

std::string BuildSearchRequestPayload(const std::string& selected_text) {
  Value payload(Value::Type::DICTIONARY);

  Value query(Value::Type::DICTIONARY);
  query.SetStringKey(kRawQueryKey, selected_text);
  payload.SetKey(kQueryKey, std::move(query));

  // TODO(llin): Change the client type.
  Value client_id(Value::Type::DICTIONARY);
  client_id.SetKey(kClientTypeKey, Value(kClientType));
  payload.SetKey(kClientIdKey, std::move(client_id));

  std::string request_payload_str;
  base::JSONWriter::Write(payload, &request_payload_str);

  return request_payload_str;
}

GURL BuildRequestUrl(const std::string& selected_text) {
  GURL result = GURL(kSearchEndpoint);

  // Add encoded request payload.
  result = net::AppendOrReplaceQueryParameter(
      result, kPayloadParam, BuildSearchRequestPayload(selected_text));
  return result;
}
}  // namespace

SearchResultLoader::SearchResultLoader(URLLoaderFactory* url_loader_factory,
                                       CompleteCallback complete_callback) {
  complete_callback_ = std::move(complete_callback);

  network_loader_factory_ = url_loader_factory;
}

SearchResultLoader::~SearchResultLoader() = default;

void SearchResultLoader::Fetch(const std::string& selected_text) {
  DCHECK(network_loader_factory_);
  DCHECK(!selected_text.empty());

  // Load the resource.
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = BuildRequestUrl(selected_text);
  loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                             kNetworkTrafficAnnotationTag);

  loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      network_loader_factory_,
      base::BindOnce(&SearchResultLoader::OnSimpleURLLoaderComplete,
                     base::Unretained(this)));
}

void SearchResultLoader::OnSimpleURLLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  if (!response_body || loader_->NetError() != net::OK ||
      !loader_->ResponseInfo() || !loader_->ResponseInfo()->headers) {
    std::move(complete_callback_).Run(/*quick_answer=*/nullptr);
    return;
  }

  search_response_parser_ =
      std::make_unique<SearchResponseParser>(std::move(complete_callback_));
  search_response_parser_->ProcessResponse(std::move(response_body));
}

}  // namespace quick_answers
}  // namespace chromeos
