// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/automated_tests/cache_replayer.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind_test_util.h"
#include "base/values.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/zlib/google/compression_utils.h"

namespace autofill {
namespace test {

using base::JSONParserOptions;
using base::JSONReader;

namespace {

constexpr char kHTTPBodySep[] = "\r\n\r\n";

// Makes HTTP request from a pair where the first element is the head and the
// second element is the body.
inline std::string MakeHTTPTextFromSplit(
    std::pair<std::string, std::string> splitted_http) {
  return base::JoinString({splitted_http.first, splitted_http.second},
                          kHTTPBodySep);
}

// Determines whether replayer should fail if there is an invalid json record.
inline bool FailOnError(int options) {
  return static_cast<bool>(options &
                           ServerCacheReplayer::kOptionFailOnInvalidJsonRecord);
}

// Determines whether replayer should fail if there is nothing to fill the cache
// with.
inline bool FailOnEmpty(int options) {
  return static_cast<bool>(options & ServerCacheReplayer::kOptionFailOnEmpty);
}

// Checks the validity of a json value node.
bool CheckNodeValidity(const base::Value* node,
                       const std::string& name,
                       base::Value::Type type) {
  if (node == nullptr) {
    VLOG(1) << "Did not find any " << name << "field in json";
    return false;
  }
  if (node->type() != type) {
    VLOG(1) << "Node value is not of type " << node->type()
            << " when it should be of type " << type;
    return false;
  }
  return true;
}

// Populates |cache| with content from a |request_node| json node that
// corresponds to an element of an Autofill URL field
// (e.g., https://clients1.google.com/tbproxy/af/query?) in the WPR capture json
// cache. Returns false when there is an error parsing |request_node|. Leaves a
// log trace whenever there is an error.
bool PopulateCacheFromJsonSingleRequestNode(const base::Value& request_node,
                                            ServerCache* cache_to_fill) {
  ServerCache& cache = *cache_to_fill;

  // Get and check "SerializedRequest" field node string.
  std::string serialized_request;
  {
    const std::string node_name = "SerializedRequest";
    const base::Value* node = request_node.FindKey(node_name);
    if (!CheckNodeValidity(node, node_name, base::Value::Type::STRING)) {
      return false;
    }
    serialized_request = node->GetString();
  }

  // Decode serialized request string.
  std::string decoded_serialized_request;
  {
    if (!base::Base64Decode(serialized_request, &decoded_serialized_request)) {
      VLOG(1) << "Could not base64 decode serialized request: "
              << serialized_request;
      return false;
    }
  }

  // Parse serialized request string to request proto and get corresponding
  // key.
  std::string key;
  {
    AutofillQueryContents query;
    if (!query.ParseFromString(SplitHTTP(decoded_serialized_request).second)) {
      VLOG(1) << "Could not parse serialized request to AutofillQueryContents: "
              << SplitHTTP(decoded_serialized_request).second;
      return false;
    }
    key = GetKeyFromQueryRequest(query);
  }

  // Get serialized response string.
  std::string serialized_response;
  {
    const std::string node_name = "SerializedResponse";
    const base::Value* node = request_node.FindKey(node_name);
    if (!CheckNodeValidity(node, node_name, base::Value::Type::STRING)) {
      return false;
    }
    serialized_response = node->GetString();
  }

  // Decode serialized response string.
  std::string decoded_serialized_response;
  {
    if (!base::Base64Decode(serialized_response,
                            &decoded_serialized_response)) {
      VLOG(1) << "Could not base64 decode serialized response, skipping cache "
                 "loading: "
              << serialized_response;
      return false;
    }
  }

  // Went through all steps, we now have compressed serialized responses we
  // can serve it back from cache.
  cache[key] = decoded_serialized_response;

  return true;
}

// Populates |cache| with content from |requests_node| (a list of single request
// nodes) json node that corresponds to an Autofill URL field
// (e.g., https://clients1.google.com/tbproxy/af/query?) in the WPR capture json
// cache. Returns false when there is an error when parsing the requests and
// OPTION_FAIL_ON_INVALID_JSON is flipped in |options|. Keeps a log trace
// whenever there is an error even if OPTION_FAIL_ON_INVALID_JSON is not
// flipped.
bool PopulateCacheFromJsonRequestsNode(const base::Value& requests_node,
                                       int options,
                                       ServerCache* cache_to_fill) {
  bool fail_on_error = FailOnError(options);
  for (const base::Value& request : requests_node.GetList()) {
    if (!PopulateCacheFromJsonSingleRequestNode(request, cache_to_fill) &&
        fail_on_error) {
      return false;
    }
  }
  return true;
}

struct QueryNode {
  // Query URL.
  std::string url = "";
  // Value node with requests mapped with |url|.
  const base::Value* node = nullptr;
};

// TODO(crbug/958125): Add the possibility to retrieve nodes with different
// Query URLs.
// Finds the Autofill server Query node in dictionary node. Gives nullptr if
// cannot find the node or |domain_dict| is invalid. The |domain_dict| has to
// outlive any usage of the returned value node pointers.
std::vector<QueryNode> FindAutofillQueryNodesInDomainDict(
    const base::Value& domain_dict) {
  if (!domain_dict.is_dict()) {
    return {};
  }
  std::vector<QueryNode> nodes;
  for (const auto& pair : domain_dict.DictItems()) {
    if (pair.first.find("https://clients1.google.com/tbproxy/af/query") !=
        std::string::npos) {
      nodes.push_back(QueryNode{pair.first, &pair.second});
    }
  }
  return nodes;
}

// Populates the cache mapping request keys to their corresponding compressed
// response.
ServerCacheReplayer::Status PopulateCacheFromJSONFile(
    const base::FilePath& json_file_path,
    int options,
    ServerCache* cache_to_fill) {
  // Read json file.
  std::string json_text;
  {
    if (!base::ReadFileToString(json_file_path, &json_text)) {
      return ServerCacheReplayer::Status{
          ServerCacheReplayer::StatusCode::kBadRead,
          "Could not read json file: "};
    }
  }

  // Decompress the json text from gzip.
  std::string decompressed_json_text;
  if (!compression::GzipUncompress(json_text, &decompressed_json_text)) {
    return ServerCacheReplayer::Status{
        ServerCacheReplayer::StatusCode::kBadRead,
        "Could not gzip decompress json in file: "};
  }

  // Parse json text content to json value node.
  base::Value root_node;
  {
    JSONReader::ValueWithError value_with_error =
        JSONReader().ReadAndReturnValueWithError(
            decompressed_json_text, JSONParserOptions::JSON_PARSE_RFC);
    if (value_with_error.error_code !=
        JSONReader::JsonParseError::JSON_NO_ERROR) {
      return ServerCacheReplayer::Status{
          ServerCacheReplayer::StatusCode::kBadRead,
          base::StrCat({"Could not load cache from json file ",
                        "because: ", value_with_error.error_message})};
    }
    if (value_with_error.value == base::nullopt) {
      return ServerCacheReplayer::Status{
          ServerCacheReplayer::StatusCode::kBadRead,
          "JSON Reader could not give any node object from json file"};
    }
    root_node = std::move(value_with_error.value.value());
  }

  {
    const char* const domain = "clients1.google.com";
    const base::Value* domain_node = root_node.FindPath({"Requests", domain});
    if (domain_node == nullptr) {
      return ServerCacheReplayer::Status{
          ServerCacheReplayer::StatusCode::kEmpty,
          base::StrCat({"there were no nodes with autofill query content in "
                        "domain node \"",
                        domain, "\""})};
    }
    std::vector<QueryNode> query_nodes =
        FindAutofillQueryNodesInDomainDict(*domain_node);

    // Fill cache with the content of each Query node. There are 3 possible
    // situations: (1) there is a single Query node that contains POST requests
    // that share the same URL, (2) there is one Query node per GET request
    // were each Query node only contains one request, and (3) a mix of (1) and
    // (2). Exit early with false whenever there is an error parsing a node.
    for (auto query_node : query_nodes) {
      if (!CheckNodeValidity(query_node.node,
                             "Requests->clients1.google.com->clients1.google."
                             "com/tbproxy/af/query*",
                             base::Value::Type::LIST)) {
        return ServerCacheReplayer::Status{
            ServerCacheReplayer::StatusCode::kBadNode,
            "could not read node content for node with URL " + query_node.url};
      }

      // Populate cache from Query node content.
      PopulateCacheFromJsonRequestsNode(*query_node.node, options,
                                        cache_to_fill);
      VLOG(1) << "Filled cache with " << query_node.node->GetList().size()
              << " requests for Query node with URL: " << query_node.url;
    }
  }

  // Return error iff there are no Query nodes and replayer is set to fail on
  // empty.
  if (cache_to_fill->empty() && FailOnEmpty(options)) {
    return ServerCacheReplayer::Status{
        ServerCacheReplayer::StatusCode::kEmpty,
        "there were no nodes with autofill query content for autofill server "
        "domains in JSON"};
  }

  return ServerCacheReplayer::Status{ServerCacheReplayer::StatusCode::kOk, ""};
}

// Gets a hexadecimal representation of a string.
std::string GetHexString(const std::string& input) {
  std::string output("0x");
  for (auto byte : input) {
    base::StringAppendF(&output, "%02x", static_cast<unsigned char>(byte));
  }
  return output;
}

// Decompressed HTTP response read from WPR capture file. Will set
// |decompressed_http| to "" and return false if there is an error.
bool DecompressHTTPResponse(const std::string& http_text,
                            std::string* decompressed_http) {
  auto header_and_body = SplitHTTP(http_text);
  if (header_and_body.first == "") {
    *decompressed_http = "";
    VLOG(1) << "Cannot decompress response of invalid HTTP text: " << http_text;
    return false;
  }
  // Look if there is a body to decompress, if not just return HTTP text as is.
  if (header_and_body.second == "") {
    *decompressed_http = http_text;
    VLOG(1) << "There is no HTTP body to decompress" << http_text;
    return true;
  }
  // TODO(crbug.com/945925): Add compression format detection, return an
  // error if not supported format.
  // Decompress the body.
  std::string decompressed_body;
  if (!compression::GzipUncompress(header_and_body.second,
                                   &decompressed_body)) {
    VLOG(1) << "Could not gzip decompress HTTP response: "
            << GetHexString(header_and_body.second);
    return false;
  }
  // Rebuild the response HTTP text by using the new decompressed body.
  *decompressed_http = MakeHTTPTextFromSplit(
      std::make_pair(std::move(header_and_body.first), decompressed_body));
  return true;
}

// Puts all data elements within the request or response body together in a
// single DataElement and returns the buffered content as a string. This ensures
// all the response body data is utilized.
std::string GetStringFromDataElements(
    const std::vector<network::DataElement>* data_elements) {
  network::DataElement unified_data_element;
  unified_data_element.SetToEmptyBytes();
  for (auto it = data_elements->begin(); it != data_elements->end(); ++it) {
    unified_data_element.AppendBytes(it->bytes(), it->length());
  }

  // Using the std::string constructor with length ensures that we don't rely
  // on having a termination character to delimit the string. This is the
  // safest approach.
  return std::string(unified_data_element.bytes(),
                     unified_data_element.length());
}

}  // namespace

// Gives a pair that contains the HTTP text split in 2, where the first
// element is the HTTP head and the second element is the HTTP body.
std::pair<std::string, std::string> SplitHTTP(std::string http_text) {
  const size_t split_index = http_text.find(kHTTPBodySep);
  if (split_index != std::string::npos) {
    const size_t sep_length = std::string(kHTTPBodySep).size();
    std::string head = http_text.substr(0, split_index);
    std::string body =
        http_text.substr(split_index + sep_length, std::string::npos);
    return std::make_pair(std::move(head), std::move(body));
  }
  return std::make_pair("", "");
}

// Gets a key for cache lookup from a query request.
std::string GetKeyFromQueryRequest(const AutofillQueryContents& query_request) {
  std::vector<std::string> form_ids;
  for (const auto& form : query_request.form()) {
    form_ids.push_back(base::NumberToString(form.signature()));
  }
  std::sort(form_ids.begin(), form_ids.end());
  return base::JoinString(form_ids, "_");
}

ServerCacheReplayer::~ServerCacheReplayer() {}

ServerCacheReplayer::ServerCacheReplayer(const base::FilePath& json_file_path,
                                         int options) {
  // Using CHECK is fine here since ServerCacheReplayer will only be used for
  // testing and we prefer the test to crash than being in an inconsistent state
  // when the cache could not be properly populated from the JSON file.
  ServerCacheReplayer::Status status =
      PopulateCacheFromJSONFile(json_file_path, options, &cache_);
  CHECK(status.Ok()) << status.message;
}

ServerCacheReplayer::ServerCacheReplayer(ServerCache server_cache)
    : cache_(std::move(server_cache)) {}

bool ServerCacheReplayer::GetResponseForQuery(
    const AutofillQueryContents& query,
    std::string* http_text) const {
  if (http_text == nullptr) {
    VLOG(1) << "Cannot fill |http_text| because null";
    return false;
  }
  std::string key = GetKeyFromQueryRequest(query);
  if (!base::Contains(const_cache_, key)) {
    VLOG(1) << "Did not match any response for " << key;
    return false;
  }
  std::string decompressed_http_response;
  // Safe to use at() here since we looked for key's presence and there is no
  // mutation done when there is concurrency.
  const std::string& http_response = const_cache_.at(key);
  if (!DecompressHTTPResponse(http_response, &decompressed_http_response)) {
    VLOG(1) << "Could not decompress http response";
    return false;
  }
  *http_text = decompressed_http_response;
  return true;
}

ServerUrlLoader::ServerUrlLoader(
    std::unique_ptr<ServerCacheReplayer> cache_replayer)
    : cache_replayer_(std::move(cache_replayer)),
      interceptor_(base::BindLambdaForTesting(
          [&](content::URLLoaderInterceptor::RequestParams* params) -> bool {
            return InterceptAutofillRequest(params);
          })) {
  // Using CHECK is fine here since ServerCacheReplayer will only be used for
  // testing and we prefer the test to crash with a CHECK rather than
  // segfaulting with a stack trace that can be hard to read.
  CHECK(cache_replayer_);
}

ServerUrlLoader::~ServerUrlLoader() {}

bool ServerUrlLoader::InterceptAutofillRequest(
    content::URLLoaderInterceptor::RequestParams* params) {
  static const char kDefaultAutofillServerQueryURL[] =
      "https://clients1.google.com/tbproxy/af/query";
  const network::ResourceRequest& resource_request = params->url_request;
  // Let all requests that are not autofill queries go to WPR.
  if (resource_request.url.spec().find(kDefaultAutofillServerQueryURL) ==
      std::string::npos) {
    return false;
  }

  // Intercept autofill query and serve back response from cache.
  // Parse HTTP request body to proto.
  VLOG(1) << "Intercepted in-flight request to Autofill Server: "
          << resource_request.url.spec();

  // TODO(crbug/958158): Extract URL content for GET Query requests.
  // Look if the body has data.
  if (resource_request.request_body == nullptr) {
    constexpr char kNoBodyHTTPErrorHeaders[] = "HTTP/2.0 400 Bad Request";
    constexpr char kNoBodyHTTPErrorBody[] =
        "there is no body data in the request";
    VLOG(1) << "Served Autofill error response: " << kNoBodyHTTPErrorBody;
    content::URLLoaderInterceptor::WriteResponse(
        std::string(kNoBodyHTTPErrorHeaders), std::string(kNoBodyHTTPErrorBody),
        params->client.get());
    return true;
  }

  std::string http_body =
      GetStringFromDataElements(resource_request.request_body->elements());
  AutofillQueryContents query_request;
  // Using CHECK is fine here since ServerCacheReplayer will only be used for
  // testing and we prefer the test to crash rather than missing the cache
  // because the HTTP body could not be parsed back to a Query request proto,
  // which can be caused by bad data in the request from the browser during
  // capture replay.
  CHECK(query_request.ParseFromString(http_body))
      << "could not parse HTTP request body to AutofillQueryContents "
         "proto: "
      << GetHexString(http_body);

  // Get response from cache using query request proto as key.
  std::string http_response;
  if (!cache_replayer_->GetResponseForQuery(query_request, &http_response)) {
    // Give back 404 error to the server if there is not match in cache.
    constexpr char kNoKeyMatchHTTPErrorHeaders[] = "HTTP/2.0 404 Not Found";
    constexpr char kNoKeyMatchHTTPErrorBody[] =
        "could not find response matching request";
    VLOG(1) << "Served Autofill error response: " << kNoKeyMatchHTTPErrorBody;
    content::URLLoaderInterceptor::WriteResponse(
        std::string(kNoKeyMatchHTTPErrorHeaders),
        std::string(kNoKeyMatchHTTPErrorBody), params->client.get());
    return true;
  }
  // Give back cache response HTTP content.
  auto http_pair = SplitHTTP(http_response);
  content::URLLoaderInterceptor::WriteResponse(
      http_pair.first, http_pair.second, params->client.get());
  VLOG(1) << "Giving back response from cache";
  return true;
}

}  // namespace test
}  // namespace autofill
