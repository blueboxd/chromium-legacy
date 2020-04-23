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
std::unique_ptr<protocol::Audits::AffectedCookie> BuildAffectedCookie(
    const mojom::blink::AffectedCookiePtr& cookie) {
  auto protocol_cookie = std::move(protocol::Audits::AffectedCookie::create()
                                       .setName(cookie->name)
                                       .setPath(cookie->path)
                                       .setDomain(cookie->domain));
  return protocol_cookie.build();
}

std::unique_ptr<protocol::Audits::AffectedRequest> BuildAffectedRequest(
    const mojom::blink::AffectedRequestPtr& request) {
  auto protocol_request = protocol::Audits::AffectedRequest::create()
                              .setRequestId(request->request_id)
                              .build();
  if (!request->url.IsEmpty()) {
    protocol_request->setUrl(request->url);
  }
  return protocol_request;
}

std::unique_ptr<protocol::Audits::AffectedFrame> BuildAffectedFrame(
    const mojom::blink::AffectedFramePtr& frame) {
  return protocol::Audits::AffectedFrame::create()
      .setFrameId(frame->frame_id)
      .build();
}

blink::protocol::String InspectorIssueCodeValue(
    mojom::blink::InspectorIssueCode code) {
  switch (code) {
    case mojom::blink::InspectorIssueCode::kSameSiteCookieIssue:
      return protocol::Audits::InspectorIssueCodeEnum::SameSiteCookieIssue;
    case mojom::blink::InspectorIssueCode::kMixedContentIssue:
      return protocol::Audits::InspectorIssueCodeEnum::MixedContentIssue;
  }
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
protocol::String BuildCookieOperation(
    mojom::blink::SameSiteCookieOperation operation) {
  switch (operation) {
    case blink::mojom::blink::SameSiteCookieOperation::SetCookie:
      return protocol::Audits::SameSiteCookieOperationEnum::SetCookie;
    case blink::mojom::blink::SameSiteCookieOperation::ReadCookie:
      return protocol::Audits::SameSiteCookieOperationEnum::ReadCookie;
  }
}

protocol::String BuildMixedContentResolutionStatus(
    mojom::blink::MixedContentResolutionStatus resolution_type) {
  switch (resolution_type) {
    case blink::mojom::blink::MixedContentResolutionStatus::MixedContentBlocked:
      return protocol::Audits::MixedContentResolutionStatusEnum::
          MixedContentBlocked;
    case blink::mojom::blink::MixedContentResolutionStatus::
        MixedContentAutomaticallyUpgraded:
      return protocol::Audits::MixedContentResolutionStatusEnum::
          MixedContentAutomaticallyUpgraded;
    case blink::mojom::blink::MixedContentResolutionStatus::MixedContentWarning:
      return protocol::Audits::MixedContentResolutionStatusEnum::
          MixedContentWarning;
  }
}

protocol::String BuildMixedContentResourceType(
    mojom::blink::RequestContextType request_context) {
  switch (request_context) {
    case blink::mojom::blink::RequestContextType::AUDIO:
      return protocol::Audits::MixedContentResourceTypeEnum::Audio;
    case blink::mojom::blink::RequestContextType::BEACON:
      return protocol::Audits::MixedContentResourceTypeEnum::Beacon;
    case blink::mojom::blink::RequestContextType::CSP_REPORT:
      return protocol::Audits::MixedContentResourceTypeEnum::CSPReport;
    case blink::mojom::blink::RequestContextType::DOWNLOAD:
      return protocol::Audits::MixedContentResourceTypeEnum::Download;
    case blink::mojom::blink::RequestContextType::EMBED:
      return protocol::Audits::MixedContentResourceTypeEnum::PluginResource;
    case blink::mojom::blink::RequestContextType::EVENT_SOURCE:
      return protocol::Audits::MixedContentResourceTypeEnum::EventSource;
    case blink::mojom::blink::RequestContextType::FAVICON:
      return protocol::Audits::MixedContentResourceTypeEnum::Favicon;
    case blink::mojom::blink::RequestContextType::FETCH:
      return protocol::Audits::MixedContentResourceTypeEnum::Resource;
    case blink::mojom::blink::RequestContextType::FONT:
      return protocol::Audits::MixedContentResourceTypeEnum::Font;
    case blink::mojom::blink::RequestContextType::FORM:
      return protocol::Audits::MixedContentResourceTypeEnum::Form;
    case blink::mojom::blink::RequestContextType::FRAME:
      return protocol::Audits::MixedContentResourceTypeEnum::Frame;
    case blink::mojom::blink::RequestContextType::HYPERLINK:
      return protocol::Audits::MixedContentResourceTypeEnum::Resource;
    case blink::mojom::blink::RequestContextType::IFRAME:
      return protocol::Audits::MixedContentResourceTypeEnum::Frame;
    case blink::mojom::blink::RequestContextType::IMAGE:
      return protocol::Audits::MixedContentResourceTypeEnum::Image;
    case blink::mojom::blink::RequestContextType::IMAGE_SET:
      return protocol::Audits::MixedContentResourceTypeEnum::Image;
    case blink::mojom::blink::RequestContextType::IMPORT:
      return protocol::Audits::MixedContentResourceTypeEnum::Import;
    case blink::mojom::blink::RequestContextType::INTERNAL:
      return protocol::Audits::MixedContentResourceTypeEnum::Resource;
    case blink::mojom::blink::RequestContextType::LOCATION:
      return protocol::Audits::MixedContentResourceTypeEnum::Resource;
    case blink::mojom::blink::RequestContextType::MANIFEST:
      return protocol::Audits::MixedContentResourceTypeEnum::Manifest;
    case blink::mojom::blink::RequestContextType::OBJECT:
      return protocol::Audits::MixedContentResourceTypeEnum::PluginResource;
    case blink::mojom::blink::RequestContextType::PING:
      return protocol::Audits::MixedContentResourceTypeEnum::Ping;
    case blink::mojom::blink::RequestContextType::PLUGIN:
      return protocol::Audits::MixedContentResourceTypeEnum::PluginData;
    case blink::mojom::blink::RequestContextType::PREFETCH:
      return protocol::Audits::MixedContentResourceTypeEnum::Prefetch;
    case blink::mojom::blink::RequestContextType::SCRIPT:
      return protocol::Audits::MixedContentResourceTypeEnum::Script;
    case blink::mojom::blink::RequestContextType::SERVICE_WORKER:
      return protocol::Audits::MixedContentResourceTypeEnum::ServiceWorker;
    case blink::mojom::blink::RequestContextType::SHARED_WORKER:
      return protocol::Audits::MixedContentResourceTypeEnum::SharedWorker;
    case blink::mojom::blink::RequestContextType::STYLE:
      return protocol::Audits::MixedContentResourceTypeEnum::Stylesheet;
    case blink::mojom::blink::RequestContextType::SUBRESOURCE:
      return protocol::Audits::MixedContentResourceTypeEnum::Resource;
    case blink::mojom::blink::RequestContextType::TRACK:
      return protocol::Audits::MixedContentResourceTypeEnum::Track;
    case blink::mojom::blink::RequestContextType::UNSPECIFIED:
      return protocol::Audits::MixedContentResourceTypeEnum::Resource;
    case blink::mojom::blink::RequestContextType::VIDEO:
      return protocol::Audits::MixedContentResourceTypeEnum::Video;
    case blink::mojom::blink::RequestContextType::WORKER:
      return protocol::Audits::MixedContentResourceTypeEnum::Worker;
    case blink::mojom::blink::RequestContextType::XML_HTTP_REQUEST:
      return protocol::Audits::MixedContentResourceTypeEnum::XMLHttpRequest;
    case blink::mojom::blink::RequestContextType::XSLT:
      return protocol::Audits::MixedContentResourceTypeEnum::XSLT;
  }
}

}  // namespace

void InspectorAuditsAgent::InspectorIssueAdded(InspectorIssue* issue) {
  auto issueDetails = protocol::Audits::InspectorIssueDetails::create();

  if (const auto* d = issue->Details()->sameSiteCookieIssueDetails.get()) {
    auto sameSiteCookieDetails =
        std::move(protocol::Audits::SameSiteCookieIssueDetails::create()
                      .setCookie(BuildAffectedCookie(d->cookie))
                      .setCookieExclusionReasons(
                          BuildCookieExclusionReasons(d->exclusionReason))
                      .setCookieWarningReasons(
                          BuildCookieWarningReasons(d->warningReason))
                      .setOperation(BuildCookieOperation(d->operation)));

    if (d->site_for_cookies) {
      sameSiteCookieDetails.setSiteForCookies(*d->site_for_cookies);
    }
    if (d->cookie_url) {
      sameSiteCookieDetails.setCookieUrl(*d->cookie_url);
    }
    if (d->request) {
      sameSiteCookieDetails.setRequest(BuildAffectedRequest(d->request));
    }
    issueDetails.setSameSiteCookieIssueDetails(sameSiteCookieDetails.build());
  }

  if (const auto* d = issue->Details()->mixed_content_issue_details.get()) {
    auto mixedContentDetails =
        protocol::Audits::MixedContentIssueDetails::create()
            .setResourceType(BuildMixedContentResourceType(d->request_context))
            .setResolutionStatus(
                BuildMixedContentResolutionStatus(d->resolution_status))
            .setInsecureURL(d->insecure_url)
            .setMainResourceURL(d->main_resource_url)
            .build();
    if (d->request) {
      mixedContentDetails->setRequest(BuildAffectedRequest(d->request));
    }
    if (d->frame) {
      mixedContentDetails->setFrame(BuildAffectedFrame(d->frame));
    }
    issueDetails.setMixedContentIssueDetails(std::move(mixedContentDetails));
  }

  auto inspector_issue = protocol::Audits::InspectorIssue::create()
                             .setCode(InspectorIssueCodeValue(issue->Code()))
                             .setDetails(issueDetails.build())
                             .build();

  GetFrontend()->issueAdded(std::move(inspector_issue));
  GetFrontend()->flush();
}

}  // namespace blink
