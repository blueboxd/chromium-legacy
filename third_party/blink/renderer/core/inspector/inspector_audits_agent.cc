// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_audits_agent.h"

#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/web/web_image.h"
#include "third_party/blink/renderer/core/inspector/inspector_network_agent.h"
#include "third_party/blink/renderer/platform/graphics/image_data_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"

#include "third_party/blink/renderer/core/inspector/inspector_issue.h"
#include "third_party/blink/renderer/core/inspector/inspector_issue_storage.h"

namespace blink {

using protocol::Maybe;
using protocol::Response;

namespace encoding_enum = protocol::Audits::GetEncodedResponse::EncodingEnum;

namespace {

static constexpr int kMaximumEncodeImageWidthInPixels = 10000;

static constexpr int kMaximumEncodeImageHeightInPixels = 10000;

static constexpr double kDefaultEncodeQuality = 1;

bool EncodeAsImage(char* body,
                   size_t size,
                   const String& encoding,
                   const double quality,
                   Vector<unsigned char>* output) {
  const WebSize maximum_size = WebSize(kMaximumEncodeImageWidthInPixels,
                                       kMaximumEncodeImageHeightInPixels);
  SkBitmap bitmap = WebImage::FromData(WebData(body, size), maximum_size);
  if (bitmap.isNull())
    return false;

  SkImageInfo info =
      SkImageInfo::Make(bitmap.width(), bitmap.height(), kRGBA_8888_SkColorType,
                        kUnpremul_SkAlphaType);
  uint32_t row_bytes = static_cast<uint32_t>(info.minRowBytes());
  Vector<unsigned char> pixel_storage(
      SafeCast<wtf_size_t>(info.computeByteSize(row_bytes)));
  SkPixmap pixmap(info, pixel_storage.data(), row_bytes);
  sk_sp<SkImage> image = SkImage::MakeFromBitmap(bitmap);

  if (!image || !image->readPixels(pixmap, 0, 0))
    return false;

  std::unique_ptr<ImageDataBuffer> image_to_encode =
      ImageDataBuffer::Create(pixmap);
  if (!image_to_encode)
    return false;

  String mime_type_name = StringView("image/") + encoding;
  ImageEncodingMimeType mime_type;
  bool valid_mime_type = ParseImageEncodingMimeType(mime_type_name, mime_type);
  DCHECK(valid_mime_type);
  return image_to_encode->EncodeImage(mime_type, quality, output);
}

}  // namespace

void InspectorAuditsAgent::Trace(Visitor* visitor) {
  visitor->Trace(network_agent_);
  visitor->Trace(inspector_issue_storage_);
  InspectorBaseAgent::Trace(visitor);
}

InspectorAuditsAgent::InspectorAuditsAgent(InspectorNetworkAgent* network_agent,
                                           InspectorIssueStorage* storage)
    : inspector_issue_storage_(storage),
      enabled_(&agent_state_, false),
      network_agent_(network_agent) {}

InspectorAuditsAgent::~InspectorAuditsAgent() = default;

protocol::Response InspectorAuditsAgent::getEncodedResponse(
    const String& request_id,
    const String& encoding,
    Maybe<double> quality,
    Maybe<bool> size_only,
    Maybe<protocol::Binary>* out_body,
    int* out_original_size,
    int* out_encoded_size) {
  DCHECK(encoding == encoding_enum::Jpeg || encoding == encoding_enum::Png ||
         encoding == encoding_enum::Webp);

  String body;
  bool is_base64_encoded;
  Response response =
      network_agent_->GetResponseBody(request_id, &body, &is_base64_encoded);
  if (!response.IsSuccess())
    return response;

  Vector<char> base64_decoded_buffer;
  if (!is_base64_encoded || !Base64Decode(body, base64_decoded_buffer) ||
      base64_decoded_buffer.size() == 0) {
    return Response::ServerError("Failed to decode original image");
  }

  Vector<unsigned char> encoded_image;
  if (!EncodeAsImage(base64_decoded_buffer.data(), base64_decoded_buffer.size(),
                     encoding, quality.fromMaybe(kDefaultEncodeQuality),
                     &encoded_image)) {
    return Response::ServerError("Could not encode image with given settings");
  }

  *out_original_size = static_cast<int>(base64_decoded_buffer.size());
  *out_encoded_size = static_cast<int>(encoded_image.size());

  if (!size_only.fromMaybe(false)) {
    *out_body = protocol::Binary::fromVector(std::move(encoded_image));
  }
  return Response::Success();
}

Response InspectorAuditsAgent::enable() {
  if (enabled_.Get()) {
    return Response::Success();
  }

  enabled_.Set(true);
  InnerEnable();
  return Response::Success();
}

Response InspectorAuditsAgent::disable() {
  if (!enabled_.Get()) {
    return Response::Success();
  }

  enabled_.Clear();
  instrumenting_agents_->RemoveInspectorAuditsAgent(this);
  return Response::Success();
}

void InspectorAuditsAgent::Restore() {
  if (!enabled_.Get())
    return;
  InnerEnable();
}

void InspectorAuditsAgent::InnerEnable() {
  instrumenting_agents_->AddInspectorAuditsAgent(this);
  for (wtf_size_t i = 0; i < inspector_issue_storage_->size(); ++i)
    InspectorIssueAdded(inspector_issue_storage_->at(i));
}

namespace {
std::unique_ptr<protocol::Array<protocol::Audits::AffectedCookie>> BuildCookies(
    const WTF::Vector<mojom::blink::AffectedCookiePtr>& cookies) {
  auto result =
      std::make_unique<protocol::Array<protocol::Audits::AffectedCookie>>();
  for (const auto& cookie : cookies) {
    auto protocol_cookie = std::move(protocol::Audits::AffectedCookie::create()
                                         .setName(cookie->name)
                                         .setPath(cookie->path)
                                         .setDomain(cookie->domain));
    if (cookie->site_for_cookies) {
      protocol_cookie.setSiteForCookies(*cookie->site_for_cookies);
    }
    result->push_back(protocol_cookie.build());
  }
  return result;
}
blink::protocol::String InspectorIssueCodeValue(
    mojom::blink::InspectorIssueCode code) {
  switch (code) {
    case mojom::blink::InspectorIssueCode::kSameSiteCookieIssue:
      return protocol::Audits::InspectorIssueCodeEnum::SameSiteCookieIssue;
  }
  NOTREACHED();
  return "unknown";
}
protocol::String BuildCookieExclusionReason(
    mojom::blink::SameSiteCookieExclusionReason exclusion_reason) {
  switch (exclusion_reason) {
    case blink::mojom::blink::SameSiteCookieExclusionReason::
        ExcludeSameSiteUnspecifiedTreatedAsLax:
      return protocol::Audits::SameSiteCookieExclusionReasonEnum::
          ExcludeSameSiteUnspecifiedTreatedAsLax;
    case blink::mojom::blink::SameSiteCookieExclusionReason::
        ExcludeSameSiteNoneInsecure:
      return protocol::Audits::SameSiteCookieExclusionReasonEnum::
          ExcludeSameSiteNoneInsecure;
  }
  NOTREACHED();
  return "unknown";
}
std::unique_ptr<std::vector<blink::protocol::String>>
BuildCookieExclusionReasons(
    const WTF::Vector<mojom::blink::SameSiteCookieExclusionReason>&
        exclusion_reasons) {
  auto protocol_exclusion_reasons =
      std::make_unique<std::vector<blink::protocol::String>>();
  for (const auto& reason : exclusion_reasons) {
    protocol_exclusion_reasons->push_back(BuildCookieExclusionReason(reason));
  }
  return protocol_exclusion_reasons;
}
protocol::String BuildCookieWarningReason(
    mojom::blink::SameSiteCookieWarningReason warning_reason) {
  switch (warning_reason) {
    case blink::mojom::blink::SameSiteCookieWarningReason::
        WarnSameSiteUnspecifiedCrossSiteContext:
      return protocol::Audits::SameSiteCookieWarningReasonEnum::
          WarnSameSiteUnspecifiedCrossSiteContext;
    case blink::mojom::blink::SameSiteCookieWarningReason::
        WarnSameSiteNoneInsecure:
      return protocol::Audits::SameSiteCookieWarningReasonEnum::
          WarnSameSiteNoneInsecure;
    case blink::mojom::blink::SameSiteCookieWarningReason::
        WarnSameSiteUnspecifiedLaxAllowUnsafe:
      return protocol::Audits::SameSiteCookieWarningReasonEnum::
          WarnSameSiteUnspecifiedLaxAllowUnsafe;
    case blink::mojom::blink::SameSiteCookieWarningReason::
        WarnSameSiteCrossSchemeSecureUrlMethodUnsafe:
      return protocol::Audits::SameSiteCookieWarningReasonEnum::
          WarnSameSiteCrossSchemeSecureUrlMethodUnsafe;
    case blink::mojom::blink::SameSiteCookieWarningReason::
        WarnSameSiteCrossSchemeSecureUrlLax:
      return protocol::Audits::SameSiteCookieWarningReasonEnum::
          WarnSameSiteCrossSchemeSecureUrlLax;
    case blink::mojom::blink::SameSiteCookieWarningReason::
        WarnSameSiteCrossSchemeSecureUrlStrict:
      return protocol::Audits::SameSiteCookieWarningReasonEnum::
          WarnSameSiteCrossSchemeSecureUrlStrict;
    case blink::mojom::blink::SameSiteCookieWarningReason::
        WarnSameSiteCrossSchemeInsecureUrlMethodUnsafe:
      return protocol::Audits::SameSiteCookieWarningReasonEnum::
          WarnSameSiteCrossSchemeInsecureUrlMethodUnsafe;
    case blink::mojom::blink::SameSiteCookieWarningReason::
        WarnSameSiteCrossSchemeInsecureUrlLax:
      return protocol::Audits::SameSiteCookieWarningReasonEnum::
          WarnSameSiteCrossSchemeInsecureUrlLax;
    case blink::mojom::blink::SameSiteCookieWarningReason::
        WarnSameSiteCrossSchemeInsecureUrlStrict:
      return protocol::Audits::SameSiteCookieWarningReasonEnum::
          WarnSameSiteCrossSchemeInsecureUrlStrict;
  }
  NOTREACHED();
  return "unknown";
}
std::unique_ptr<std::vector<blink::protocol::String>> BuildCookieWarningReasons(
    const WTF::Vector<mojom::blink::SameSiteCookieWarningReason>&
        warning_reasons) {
  auto protocol_warning_reasons =
      std::make_unique<std::vector<blink::protocol::String>>();
  for (const auto& reason : warning_reasons) {
    protocol_warning_reasons->push_back(BuildCookieWarningReason(reason));
  }
  return protocol_warning_reasons;
}
}  // namespace

void InspectorAuditsAgent::InspectorIssueAdded(InspectorIssue* issue) {
  auto issueDetails = protocol::Audits::InspectorIssueDetails::create();

  if (issue->Details()->sameSiteCookieIssueDetails) {
    auto sameSiteCookieDetails =
        protocol::Audits::SameSiteCookieIssueDetails::create()
            .setCookieExclusionReasons(BuildCookieExclusionReasons(
                issue->Details()->sameSiteCookieIssueDetails->exclusionReason))
            .setCookieWarningReasons(BuildCookieWarningReasons(
                issue->Details()->sameSiteCookieIssueDetails->warningReason))
            .build();
    issueDetails.setSameSiteCookieIssueDetails(
        std::move(sameSiteCookieDetails));
  }

  auto affectedResources =
      protocol::Audits::AffectedResources::create()
          .setCookies(BuildCookies(issue->Resources()->cookies))
          .build();

  auto inspector_issue = protocol::Audits::InspectorIssue::create()
                             .setCode(InspectorIssueCodeValue(issue->Code()))
                             .setDetails(issueDetails.build())
                             .setResources(std::move(affectedResources))
                             .build();

  GetFrontend()->issueAdded(std::move(inspector_issue));
  GetFrontend()->flush();
}

}  // namespace blink
