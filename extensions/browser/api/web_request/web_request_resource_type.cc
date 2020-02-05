// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/web_request_resource_type.h"

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/stl_util.h"
#include "extensions/browser/api/web_request/web_request_info.h"

namespace extensions {

namespace {

constexpr struct {
  const char* const name;
  const WebRequestResourceType type;
} kResourceTypes[] = {
    {"main_frame", WebRequestResourceType::MAIN_FRAME},
    {"sub_frame", WebRequestResourceType::SUB_FRAME},
    {"stylesheet", WebRequestResourceType::STYLESHEET},
    {"script", WebRequestResourceType::SCRIPT},
    {"image", WebRequestResourceType::IMAGE},
    {"font", WebRequestResourceType::FONT},
    {"object", WebRequestResourceType::OBJECT},
    {"xmlhttprequest", WebRequestResourceType::XHR},
    {"ping", WebRequestResourceType::PING},
    {"csp_report", WebRequestResourceType::CSP_REPORT},
    {"media", WebRequestResourceType::MEDIA},
    {"websocket", WebRequestResourceType::WEB_SOCKET},
    {"other", WebRequestResourceType::OTHER},
};

constexpr size_t kResourceTypesLength = base::size(kResourceTypes);

static_assert(kResourceTypesLength ==
                  base::strict_cast<size_t>(WebRequestResourceType::OTHER) + 1,
              "Each WebRequestResourceType should have a string name.");

}  // namespace

WebRequestResourceType ToWebRequestResourceType(
    blink::mojom::ResourceType type) {
  switch (type) {
    case blink::mojom::ResourceType::kMainFrame:
    case blink::mojom::ResourceType::kNavigationPreloadMainFrame:
      return WebRequestResourceType::MAIN_FRAME;
    case blink::mojom::ResourceType::kSubFrame:
    case blink::mojom::ResourceType::kNavigationPreloadSubFrame:
      return WebRequestResourceType::SUB_FRAME;
    case blink::mojom::ResourceType::kStylesheet:
      return WebRequestResourceType::STYLESHEET;
    case blink::mojom::ResourceType::kScript:
      return WebRequestResourceType::SCRIPT;
    case blink::mojom::ResourceType::kImage:
      return WebRequestResourceType::IMAGE;
    case blink::mojom::ResourceType::kFontResource:
      return WebRequestResourceType::FONT;
    case blink::mojom::ResourceType::kSubResource:
      return WebRequestResourceType::OTHER;
    case blink::mojom::ResourceType::kObject:
      return WebRequestResourceType::OBJECT;
    case blink::mojom::ResourceType::kMedia:
      return WebRequestResourceType::MEDIA;
    case blink::mojom::ResourceType::kWorker:
    case blink::mojom::ResourceType::kSharedWorker:
      return WebRequestResourceType::SCRIPT;
    case blink::mojom::ResourceType::kPrefetch:
      return WebRequestResourceType::OTHER;
    case blink::mojom::ResourceType::kFavicon:
      return WebRequestResourceType::IMAGE;
    case blink::mojom::ResourceType::kXhr:
      return WebRequestResourceType::XHR;
    case blink::mojom::ResourceType::kPing:
      return WebRequestResourceType::PING;
    case blink::mojom::ResourceType::kServiceWorker:
      return WebRequestResourceType::SCRIPT;
    case blink::mojom::ResourceType::kCspReport:
      return WebRequestResourceType::CSP_REPORT;
    case blink::mojom::ResourceType::kPluginResource:
      return WebRequestResourceType::OBJECT;
  }
  NOTREACHED();
  return WebRequestResourceType::OTHER;
}

const char* WebRequestResourceTypeToString(WebRequestResourceType type) {
  size_t index = base::strict_cast<size_t>(type);
  DCHECK_LT(index, kResourceTypesLength);
  DCHECK_EQ(kResourceTypes[index].type, type);
  return kResourceTypes[index].name;
}

bool ParseWebRequestResourceType(base::StringPiece text,
                                 WebRequestResourceType* type) {
  for (size_t i = 0; i < kResourceTypesLength; ++i) {
    if (text == kResourceTypes[i].name) {
      *type = kResourceTypes[i].type;
      DCHECK_EQ(static_cast<WebRequestResourceType>(i), *type);
      return true;
    }
  }
  return false;
}

}  // namespace extensions
