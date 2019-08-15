/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann (hausmann@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006, 2010 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/html/html_html_element.h"

#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_parser.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/loader/appcache/application_cache_host_for_frame.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

using namespace html_names;

HTMLHtmlElement::HTMLHtmlElement(Document& document)
    : HTMLElement(kHTMLTag, document) {}

bool HTMLHtmlElement::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName() == kManifestAttr ||
         HTMLElement::IsURLAttribute(attribute);
}

void HTMLHtmlElement::InsertedByParser() {
  // When parsing a fragment, its dummy document has a null parser.
  if (!GetDocument().Parser())
    return;

  MaybeSetupApplicationCache();
  if (!GetDocument().Parser())
    return;

  GetDocument().Parser()->DocumentElementAvailable();
  if (GetDocument().GetFrame()) {
    GetDocument().GetFrame()->Loader().DispatchDocumentElementAvailable();
    GetDocument().GetFrame()->Loader().RunScriptsAtDocumentElementAvailable();
    // RunScriptsAtDocumentElementAvailable might have invalidated
    // GetDocument().
  }
}

void HTMLHtmlElement::MaybeSetupApplicationCache() {
  if (!GetDocument().GetFrame())
    return;

  DocumentLoader* document_loader =
      GetDocument().GetFrame()->Loader().GetDocumentLoader();
  if (!document_loader ||
      !GetDocument().Parser()->DocumentWasLoadedAsPartOfNavigation())
    return;
  const AtomicString& manifest = FastGetAttribute(kManifestAttr);

  if (RuntimeEnabledFeatures::RestrictAppCacheToSecureContextsEnabled() &&
      !GetDocument().IsSecureContext()) {
    if (!manifest.IsEmpty()) {
      Deprecation::CountDeprecation(
          GetDocument(), WebFeature::kApplicationCacheAPIInsecureOrigin);
    }
    return;
  }

  ApplicationCacheHostForFrame* host =
      document_loader->GetApplicationCacheHost();
  DCHECK(host);

  if (manifest.IsEmpty())
    host->SelectCacheWithoutManifest();
  else
    host->SelectCacheWithManifest(GetDocument().CompleteURL(manifest));
  bool app_cache_installed =
      host->GetStatus() !=
      blink::mojom::AppCacheStatus::APPCACHE_STATUS_UNCACHED;
  if (app_cache_installed && manifest.IsEmpty()) {
    UseCounter::Count(GetDocument(),
                      WebFeature::kApplicationCacheInstalledButNoManifest);
  }
}

const CSSPropertyValueSet*
HTMLHtmlElement::AdditionalPresentationAttributeStyle() {
  if (const CSSValue* color_scheme =
          GetDocument().GetStyleEngine().GetMetaColorSchemeValue()) {
    DEFINE_STATIC_LOCAL(
        Persistent<MutableCSSPropertyValueSet>, color_scheme_style,
        (MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode)));
    color_scheme_style->SetProperty(CSSPropertyID::kColorScheme, *color_scheme);
    return color_scheme_style;
  }
  return nullptr;
}

}  // namespace blink
