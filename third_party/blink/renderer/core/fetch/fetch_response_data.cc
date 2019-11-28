// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/fetch_response_data.h"

#include "services/network/public/cpp/content_security_policy.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/content_security_policy.mojom-blink.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_response.mojom-blink.h"
#include "third_party/blink/renderer/core/fetch/fetch_header_list.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/loader/cors/cors.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_utils.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "url/gurl.h"

using Type = network::mojom::FetchResponseType;
using ResponseSource = network::mojom::FetchResponseSource;

// TODO(lfg): Stop converting from/to blink type. Instead use mojo to
// automagically convert this.
namespace network {
namespace mojom {

blink::CSPSourcePtr ConvertToBlink(CSPSourcePtr source) {
  return blink::CSPSource::New(
      String::FromUTF8(source->scheme), String::FromUTF8(source->host),
      source->port, String::FromUTF8(source->path), source->is_host_wildcard,
      source->is_port_wildcard);
}

blink::CSPSourceListPtr ConvertToBlink(CSPSourceListPtr source_list) {
  WTF::Vector<blink::CSPSourcePtr> sources;
  for (auto& it : source_list->sources)
    sources.push_back(ConvertToBlink(std::move(it)));

  return blink::CSPSourceList::New(std::move(sources), source_list->allow_self,
                                   source_list->allow_star);
}

blink::CSPDirective::Name ConvertToBlink(CSPDirective::Name name) {
  return static_cast<blink::CSPDirective::Name>(name);
}

blink::CSPDirectivePtr ConvertToBlink(CSPDirectivePtr csp) {
  return blink::CSPDirective::New(ConvertToBlink(csp->name),
                                  ConvertToBlink(std::move(csp->source_list)));
}

blink::ContentSecurityPolicyPtr ConvertToBlink(ContentSecurityPolicyPtr csp) {
  WTF::Vector<blink::CSPDirectivePtr> directives;
  for (auto& directive : csp->directives)
    directives.push_back(ConvertToBlink(std::move(directive)));

  WTF::Vector<WTF::String> report_endpoints;
  for (auto& endpoint : csp->report_endpoints)
    report_endpoints.push_back(String::FromUTF8(endpoint));

  return blink::ContentSecurityPolicy::New(std::move(directives),
                                           csp->use_reporting_api,
                                           std::move(report_endpoints));
}

}  // namespace mojom
}  // namespace network

namespace blink {

namespace {

Vector<String> HeaderSetToVector(const WebHTTPHeaderSet& headers) {
  Vector<String> result;
  result.ReserveInitialCapacity(SafeCast<wtf_size_t>(headers.size()));
  // WebHTTPHeaderSet stores headers using Latin1 encoding.
  for (const auto& header : headers)
    result.push_back(String(header.data(), header.size()));
  return result;
}

}  // namespace

FetchResponseData* FetchResponseData::Create() {
  // "Unless stated otherwise, a response's url is null, status is 200, status
  // message is the empty byte sequence, header list is an empty header list,
  // and body is null."
  return MakeGarbageCollected<FetchResponseData>(
      Type::kDefault, ResponseSource::kUnspecified, 200, g_empty_atom);
}

FetchResponseData* FetchResponseData::CreateNetworkErrorResponse() {
  // "A network error is a response whose status is always 0, status message
  // is always the empty byte sequence, header list is aways an empty list,
  // and body is always null."
  return MakeGarbageCollected<FetchResponseData>(
      Type::kError, ResponseSource::kUnspecified, 0, g_empty_atom);
}

FetchResponseData* FetchResponseData::CreateWithBuffer(
    BodyStreamBuffer* buffer) {
  FetchResponseData* response = FetchResponseData::Create();
  response->buffer_ = buffer;
  return response;
}

FetchResponseData* FetchResponseData::CreateBasicFilteredResponse() const {
  DCHECK_EQ(type_, Type::kDefault);
  // "A basic filtered response is a filtered response whose type is |basic|,
  // header list excludes any headers in internal response's header list whose
  // name is `Set-Cookie` or `Set-Cookie2`."
  FetchResponseData* response = MakeGarbageCollected<FetchResponseData>(
      Type::kBasic, response_source_, status_, status_message_);
  response->SetURLList(url_list_);
  for (const auto& header : header_list_->List()) {
    if (FetchUtils::IsForbiddenResponseHeaderName(header.first))
      continue;
    response->header_list_->Append(header.first, header.second);
  }
  response->buffer_ = buffer_;
  response->mime_type_ = mime_type_;
  response->internal_response_ = const_cast<FetchResponseData*>(this);
  return response;
}

FetchResponseData* FetchResponseData::CreateCorsFilteredResponse(
    const WebHTTPHeaderSet& exposed_headers) const {
  DCHECK_EQ(type_, Type::kDefault);
  // "A CORS filtered response is a filtered response whose type is |CORS|,
  // header list excludes all headers in internal response's header list,
  // except those whose name is either one of `Cache-Control`,
  // `Content-Language`, `Content-Type`, `Expires`, `Last-Modified`, and
  // `Pragma`, and except those whose name is one of the values resulting from
  // parsing `Access-Control-Expose-Headers` in internal response's header
  // list."
  FetchResponseData* response = MakeGarbageCollected<FetchResponseData>(
      Type::kCors, response_source_, status_, status_message_);
  response->SetURLList(url_list_);
  for (const auto& header : header_list_->List()) {
    const String& name = header.first;
    if (cors::IsCorsSafelistedResponseHeader(name) ||
        (exposed_headers.find(name.Ascii()) != exposed_headers.end() &&
         !FetchUtils::IsForbiddenResponseHeaderName(name))) {
      response->header_list_->Append(name, header.second);
    }
  }
  response->cors_exposed_header_names_ = exposed_headers;
  response->buffer_ = buffer_;
  response->mime_type_ = mime_type_;
  response->internal_response_ = const_cast<FetchResponseData*>(this);
  return response;
}

FetchResponseData* FetchResponseData::CreateOpaqueFilteredResponse() const {
  DCHECK_EQ(type_, Type::kDefault);
  // "An opaque filtered response is a filtered response whose type is
  // 'opaque', url list is the empty list, status is 0, status message is the
  // empty byte sequence, header list is the empty list, body is null, and
  // cache state is 'none'."
  //
  // https://fetch.spec.whatwg.org/#concept-filtered-response-opaque
  FetchResponseData* response = MakeGarbageCollected<FetchResponseData>(
      Type::kOpaque, response_source_, 0, g_empty_atom);
  response->internal_response_ = const_cast<FetchResponseData*>(this);
  return response;
}

FetchResponseData* FetchResponseData::CreateOpaqueRedirectFilteredResponse()
    const {
  DCHECK_EQ(type_, Type::kDefault);
  // "An opaque filtered response is a filtered response whose type is
  // 'opaqueredirect', status is 0, status message is the empty byte sequence,
  // header list is the empty list, body is null, and cache state is 'none'."
  //
  // https://fetch.spec.whatwg.org/#concept-filtered-response-opaque-redirect
  FetchResponseData* response = MakeGarbageCollected<FetchResponseData>(
      Type::kOpaqueRedirect, response_source_, 0, g_empty_atom);
  response->SetURLList(url_list_);
  response->internal_response_ = const_cast<FetchResponseData*>(this);
  return response;
}

const KURL* FetchResponseData::Url() const {
  // "A response has an associated url. It is a pointer to the last response URL
  // in response’s url list and null if response’s url list is the empty list."
  if (url_list_.IsEmpty())
    return nullptr;
  return &url_list_.back();
}

FetchHeaderList* FetchResponseData::InternalHeaderList() const {
  if (internal_response_) {
    return internal_response_->HeaderList();
  }
  return HeaderList();
}

String FetchResponseData::MimeType() const {
  return mime_type_;
}

BodyStreamBuffer* FetchResponseData::InternalBuffer() const {
  if (internal_response_) {
    return internal_response_->buffer_;
  }
  return buffer_;
}

String FetchResponseData::InternalMIMEType() const {
  if (internal_response_) {
    return internal_response_->MimeType();
  }
  return mime_type_;
}

void FetchResponseData::SetURLList(const Vector<KURL>& url_list) {
  url_list_ = url_list;
}

const Vector<KURL>& FetchResponseData::InternalURLList() const {
  if (internal_response_) {
    return internal_response_->url_list_;
  }
  return url_list_;
}

FetchResponseData* FetchResponseData::Clone(ScriptState* script_state,
                                            ExceptionState& exception_state) {
  FetchResponseData* new_response = Create();
  new_response->type_ = type_;
  new_response->response_source_ = response_source_;
  if (termination_reason_) {
    new_response->termination_reason_ = std::make_unique<TerminationReason>();
    *new_response->termination_reason_ = *termination_reason_;
  }
  new_response->SetURLList(url_list_);
  new_response->status_ = status_;
  new_response->status_message_ = status_message_;
  new_response->header_list_ = header_list_->Clone();
  new_response->mime_type_ = mime_type_;
  new_response->response_time_ = response_time_;
  new_response->cache_storage_cache_name_ = cache_storage_cache_name_;
  new_response->cors_exposed_header_names_ = cors_exposed_header_names_;

  switch (type_) {
    case Type::kBasic:
    case Type::kCors:
      DCHECK(internal_response_);
      DCHECK_EQ(buffer_, internal_response_->buffer_);
      DCHECK_EQ(internal_response_->type_, Type::kDefault);
      new_response->internal_response_ =
          internal_response_->Clone(script_state, exception_state);
      if (exception_state.HadException())
        return nullptr;
      buffer_ = internal_response_->buffer_;
      new_response->buffer_ = new_response->internal_response_->buffer_;
      break;
    case Type::kDefault: {
      DCHECK(!internal_response_);
      if (buffer_) {
        BodyStreamBuffer* new1 = nullptr;
        BodyStreamBuffer* new2 = nullptr;
        buffer_->Tee(&new1, &new2, exception_state);
        if (exception_state.HadException())
          return nullptr;
        buffer_ = new1;
        new_response->buffer_ = new2;
      }
      break;
    }
    case Type::kError:
      DCHECK(!internal_response_);
      DCHECK(!buffer_);
      break;
    case Type::kOpaque:
    case Type::kOpaqueRedirect:
      DCHECK(internal_response_);
      DCHECK(!buffer_);
      DCHECK_EQ(internal_response_->type_, Type::kDefault);
      new_response->internal_response_ =
          internal_response_->Clone(script_state, exception_state);
      if (exception_state.HadException())
        return nullptr;
      break;
  }
  return new_response;
}

mojom::blink::FetchAPIResponsePtr FetchResponseData::PopulateFetchAPIResponse(
    const KURL& request_url) {
  if (internal_response_) {
    mojom::blink::FetchAPIResponsePtr response =
        internal_response_->PopulateFetchAPIResponse(request_url);
    response->response_type = type_;
    response->response_source = response_source_;
    response->cors_exposed_header_names =
        HeaderSetToVector(cors_exposed_header_names_);
    return response;
  }
  mojom::blink::FetchAPIResponsePtr response =
      mojom::blink::FetchAPIResponse::New();
  response->url_list = url_list_;
  response->status_code = status_;
  response->status_text = status_message_;
  response->response_type = type_;
  response->response_source = response_source_;
  response->response_time = response_time_;
  response->cache_storage_cache_name = cache_storage_cache_name_;
  response->cors_exposed_header_names =
      HeaderSetToVector(cors_exposed_header_names_);
  response->side_data_blob = side_data_blob_;
  for (const auto& header : HeaderList()->List())
    response->headers.insert(header.first, header.second);

  // Check if there's a Content-Security-Policy header and parse it if
  // necessary.
  // TODO(lfg). What about report only header?
  if (base::FeatureList::IsEnabled(
          network::features::kOutOfBlinkFrameAncestors)) {
    String content_security_policy_header;
    if (HeaderList()->Get("content-security-policy",
                          content_security_policy_header)) {
      network::ContentSecurityPolicy policy;
      if (policy.Parse(request_url,
                       StringUTF8Adaptor(content_security_policy_header)
                           .AsStringPiece())) {
        response->content_security_policy =
            ConvertToBlink(policy.TakeContentSecurityPolicy());
      }
    }
  }
  return response;
}

FetchResponseData::FetchResponseData(Type type,
                                     network::mojom::FetchResponseSource source,
                                     uint16_t status,
                                     AtomicString status_message)
    : type_(type),
      response_source_(source),
      status_(status),
      status_message_(status_message),
      header_list_(MakeGarbageCollected<FetchHeaderList>()),
      response_time_(base::Time::Now()) {}

void FetchResponseData::ReplaceBodyStreamBuffer(BodyStreamBuffer* buffer) {
  if (type_ == Type::kBasic || type_ == Type::kCors) {
    DCHECK(internal_response_);
    internal_response_->buffer_ = buffer;
    buffer_ = buffer;
  } else if (type_ == Type::kDefault) {
    DCHECK(!internal_response_);
    buffer_ = buffer;
  }
}

void FetchResponseData::Trace(blink::Visitor* visitor) {
  visitor->Trace(header_list_);
  visitor->Trace(internal_response_);
  visitor->Trace(buffer_);
}

}  // namespace blink
