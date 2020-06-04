// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/savable_resources.h"

#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_element_collection.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace {

// Returns |true| if |web_frame| contains (or should be assumed to contain)
// a html document.
bool DoesFrameContainHtmlDocument(WebFrame* web_frame,
                                  const WebElement& element) {
  if (web_frame->IsWebLocalFrame()) {
    WebDocument doc = web_frame->ToWebLocalFrame()->GetDocument();
    return doc.IsHTMLDocument() || doc.IsXHTMLDocument();
  }

  // Cannot inspect contents of a remote frame, so we use a heuristic:
  // Assume that <iframe> and <frame> elements contain a html document,
  // and other elements (i.e. <object>) contain plugins or other resources.
  // If the heuristic is wrong (i.e. the remote frame in <object> does
  // contain an html document), then things will still work, but with the
  // following caveats: 1) original frame content will be saved and 2) links
  // in frame's html doc will not be rewritten to point to locally saved
  // files.
  return element.HasHTMLTagName("iframe") || element.HasHTMLTagName("frame");
}

// If present and valid, then push the link associated with |element|
// into either WebSavableResources::ResultImpl::subframes or
// WebSavableResources::ResultImpl::resources_list.
void GetSavableResourceLinkForElement(const WebElement& element,
                                      const WebDocument& current_doc,
                                      SavableResources::Result* result) {
  // Get absolute URL.
  String link_attribute_value =
      SavableResources::GetSubResourceLinkFromElement(element);
  KURL element_url = current_doc.CompleteURL(link_attribute_value);

  // See whether to report this element as a subframe.
  WebFrame* web_frame = WebFrame::FromFrameOwnerElement(element);
  if (web_frame && DoesFrameContainHtmlDocument(web_frame, element)) {
    mojom::blink::SavableSubframePtr subframe =
        mojom::blink::SavableSubframe::New(element_url,
                                           web_frame->GetFrameToken());
    result->AppendSubframe(std::move(subframe));
    return;
  }

  // Check whether the node has sub resource URL or not.
  if (link_attribute_value.IsNull())
    return;

  // Ignore invalid URL.
  if (!element_url.IsValid())
    return;

  // Ignore those URLs which are not standard protocols. Because FTP
  // protocol does no have cache mechanism, we will skip all
  // sub-resources if they use FTP protocol.
  if (!element_url.ProtocolIsInHTTPFamily() &&
      !element_url.ProtocolIs(url::kFileScheme))
    return;

  result->AppendResourceLink(element_url);
}

}  // namespace

// static
bool SavableResources::GetSavableResourceLinksForFrame(
    LocalFrame* current_frame,
    SavableResources::Result* result) {
  // Get current frame's URL.
  KURL current_frame_url = current_frame->GetDocument()->Url();

  // If url of current frame is invalid, ignore it.
  if (!current_frame_url.IsValid())
    return false;

  // If url of current frame is not a savable protocol, ignore it.
  if (!Platform::Current()->IsURLSavableForSavableResource(current_frame_url))
    return false;

  // Get current using document.
  WebDocument current_doc = current_frame->GetDocument();
  // Go through all descent nodes.
  WebElementCollection all = current_doc.All();
  // Go through all elements in this frame.
  for (WebElement element = all.FirstItem(); !element.IsNull();
       element = all.NextItem()) {
    GetSavableResourceLinkForElement(element, current_doc, result);
  }

  return true;
}

// static
String SavableResources::GetSubResourceLinkFromElement(
    const WebElement& element) {
  const char* attribute_name = nullptr;
  if (element.HasHTMLTagName("img") || element.HasHTMLTagName("frame") ||
      element.HasHTMLTagName("iframe") || element.HasHTMLTagName("script")) {
    attribute_name = "src";
  } else if (element.HasHTMLTagName("input")) {
    const WebInputElement input = element.ToConst<WebInputElement>();
    if (input.IsImageButton()) {
      attribute_name = "src";
    }
  } else if (element.HasHTMLTagName("body") ||
             element.HasHTMLTagName("table") || element.HasHTMLTagName("tr") ||
             element.HasHTMLTagName("td")) {
    attribute_name = "background";
  } else if (element.HasHTMLTagName("blockquote") ||
             element.HasHTMLTagName("q") || element.HasHTMLTagName("del") ||
             element.HasHTMLTagName("ins")) {
    attribute_name = "cite";
  } else if (element.HasHTMLTagName("object")) {
    attribute_name = "data";
  } else if (element.HasHTMLTagName("link")) {
    // If the link element is not linked to css, ignore it.
    String type = element.GetAttribute("type");
    String rel = element.GetAttribute("rel");
    if ((type.ContainsOnlyASCIIOrEmpty() && type.LowerASCII() == "text/css") ||
        (rel.ContainsOnlyASCIIOrEmpty() && rel.LowerASCII() == "stylesheet")) {
      // TODO(jnd): Add support for extracting links of sub-resources which
      // are inside style-sheet such as @import, url(), etc.
      // See bug: http://b/issue?id=1111667.
      attribute_name = "href";
    }
  }
  if (!attribute_name)
    return String();
  String value = element.GetAttribute(String::FromUTF8(attribute_name));
  // If value has content and not start with "javascript:" then return it,
  // otherwise return an empty string.
  if (!value.IsNull() && !value.IsEmpty() &&
      !value.StartsWith("javascript:", kTextCaseASCIIInsensitive))
    return value;

  return String();
}

void SavableResources::Result::AppendSubframe(
    mojom::blink::SavableSubframePtr subframe) {
  subframes_->emplace_back(std::move(subframe));
}

void SavableResources::Result::AppendResourceLink(const KURL& url) {
  resources_list_->emplace_back(url);
}

}  // namespace blink
