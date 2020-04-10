/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Samuel Weinig (sam@webkit.org)
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/dom/dom_implementation.h"

#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/media_list.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/context_features.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/dom/document_type.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/sink_document.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/dom/xml_document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_registration_context.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_title_element.h"
#include "third_party/blink/renderer/core/html/html_view_source_document.h"
#include "third_party/blink/renderer/core/html/image_document.h"
#include "third_party/blink/renderer/core/html/media/media_document.h"
#include "third_party/blink/renderer/core/html/plugin_document.h"
#include "third_party/blink/renderer/core/html/text_document.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/network/mime/content_type.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

DOMImplementation::DOMImplementation(Document& document)
    : document_(document) {}

DocumentType* DOMImplementation::createDocumentType(
    const AtomicString& qualified_name,
    const String& public_id,
    const String& system_id,
    ExceptionState& exception_state) {
  AtomicString prefix, local_name;
  if (!Document::ParseQualifiedName(qualified_name, prefix, local_name,
                                    exception_state))
    return nullptr;

  return MakeGarbageCollected<DocumentType>(document_, qualified_name,
                                            public_id, system_id);
}

XMLDocument* DOMImplementation::createDocument(
    const AtomicString& namespace_uri,
    const AtomicString& qualified_name,
    DocumentType* doctype,
    ExceptionState& exception_state) {
  XMLDocument* doc = nullptr;
  DocumentInit init = DocumentInit::Create()
                          .WithContextDocument(document_->ContextDocument())
                          .WithOwnerDocument(document_->ContextDocument());
  if (namespace_uri == svg_names::kNamespaceURI) {
    doc = XMLDocument::CreateSVG(init);
  } else if (namespace_uri == html_names::xhtmlNamespaceURI) {
    doc = XMLDocument::CreateXHTML(
        init.WithRegistrationContext(document_->RegistrationContext()));
  } else {
    doc = MakeGarbageCollected<XMLDocument>(init);
  }

  doc->SetContextFeatures(document_->GetContextFeatures());

  Node* document_element = nullptr;
  if (!qualified_name.IsEmpty()) {
    document_element =
        doc->createElementNS(namespace_uri, qualified_name, exception_state);
    if (exception_state.HadException())
      return nullptr;
  }

  if (doctype)
    doc->AppendChild(doctype);
  if (document_element)
    doc->AppendChild(document_element);

  return doc;
}

bool DOMImplementation::IsXMLMIMEType(const String& mime_type) {
  if (EqualIgnoringASCIICase(mime_type, "text/xml") ||
      EqualIgnoringASCIICase(mime_type, "application/xml") ||
      EqualIgnoringASCIICase(mime_type, "text/xsl"))
    return true;

  // Per RFCs 3023 and 2045, an XML MIME type is of the form:
  // ^[0-9a-zA-Z_\\-+~!$\\^{}|.%'`#&*]+/[0-9a-zA-Z_\\-+~!$\\^{}|.%'`#&*]+\+xml$

  int length = mime_type.length();
  if (length < 7)
    return false;

  if (mime_type[0] == '/' || mime_type[length - 5] == '/' ||
      !mime_type.EndsWithIgnoringASCIICase("+xml"))
    return false;

  bool has_slash = false;
  for (int i = 0; i < length - 4; ++i) {
    UChar ch = mime_type[i];
    if (ch >= '0' && ch <= '9')
      continue;
    if (ch >= 'a' && ch <= 'z')
      continue;
    if (ch >= 'A' && ch <= 'Z')
      continue;
    switch (ch) {
      case '_':
      case '-':
      case '+':
      case '~':
      case '!':
      case '$':
      case '^':
      case '{':
      case '}':
      case '|':
      case '.':
      case '%':
      case '\'':
      case '`':
      case '#':
      case '&':
      case '*':
        continue;
      case '/':
        if (has_slash)
          return false;
        has_slash = true;
        continue;
      default:
        return false;
    }
  }

  return true;
}

static bool IsTextPlainType(const String& mime_type) {
  return mime_type.StartsWithIgnoringASCIICase("text/") &&
         !(EqualIgnoringASCIICase(mime_type, "text/html") ||
           EqualIgnoringASCIICase(mime_type, "text/xml") ||
           EqualIgnoringASCIICase(mime_type, "text/xsl"));
}

bool DOMImplementation::IsTextMIMEType(const String& mime_type) {
  return MIMETypeRegistry::IsSupportedJavaScriptMIMEType(mime_type) ||
         MIMETypeRegistry::IsJSONMimeType(mime_type) ||
         IsTextPlainType(mime_type);
}

Document* DOMImplementation::createHTMLDocument(const String& title) {
  DocumentInit init =
      DocumentInit::Create()
          .WithContextDocument(document_->ContextDocument())
          .WithOwnerDocument(document_->ContextDocument())
          .WithRegistrationContext(document_->RegistrationContext())
          .WithContentSecurityPolicyFromContextDoc();
  auto* d = MakeGarbageCollected<HTMLDocument>(init);
  d->open();
  d->write("<!doctype html><html><head></head><body></body></html>");
  if (!title.IsNull()) {
    HTMLHeadElement* head_element = d->head();
    DCHECK(head_element);
    auto* title_element = MakeGarbageCollected<HTMLTitleElement>(*d);
    head_element->AppendChild(title_element);
    title_element->AppendChild(d->createTextNode(title), ASSERT_NO_EXCEPTION);
  }
  d->SetContextFeatures(document_->GetContextFeatures());
  return d;
}

Document* DOMImplementation::createDocument(const DocumentInit& init) {
  switch (init.GetType()) {
    case DocumentInit::Type::kHTML:
      return MakeGarbageCollected<HTMLDocument>(init);
    case DocumentInit::Type::kXHTML:
      return XMLDocument::CreateXHTML(init);
    case DocumentInit::Type::kImage:
      return MakeGarbageCollected<ImageDocument>(init);
    case DocumentInit::Type::kPlugin: {
      Document* document = MakeGarbageCollected<PluginDocument>(init);
      // TODO(crbug.com/1029822): Final sandbox flags are calculated during
      // document construction, so we have to construct a PluginDocument then
      // replace it with a SinkDocument when plugins are sanboxed. If we move
      // final sandbox flag calcuation earlier, we could construct the
      // SinkDocument directly.
      if (document->IsSandboxed(
              network::mojom::blink::WebSandboxFlags::kPlugins))
        document = MakeGarbageCollected<SinkDocument>(init);
      return document;
    }
    case DocumentInit::Type::kMedia:
      return MakeGarbageCollected<MediaDocument>(init);
    case DocumentInit::Type::kSVG:
      return XMLDocument::CreateSVG(init);
    case DocumentInit::Type::kXML:
      return MakeGarbageCollected<XMLDocument>(init);
    case DocumentInit::Type::kViewSource:
      return MakeGarbageCollected<HTMLViewSourceDocument>(init);
    case DocumentInit::Type::kText:
      return MakeGarbageCollected<TextDocument>(init);
    case DocumentInit::Type::kUnspecified:
      FALLTHROUGH;
    default:
      break;
  }
  NOTREACHED();
  return nullptr;
}

void DOMImplementation::Trace(Visitor* visitor) {
  visitor->Trace(document_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
