/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004-2008, 2013, 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (C) 2011 Motorola Mobility. All rights reserved.
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
 *
 */

#include "third_party/blink/renderer/core/html/html_element.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/bindings/core/v8/script_event_listener.h"
#include "third_party/blink/renderer/bindings/core/v8/string_or_trusted_script.h"
#include "third_party/blink/renderer/bindings/core/v8/string_treat_null_as_empty_string_or_trusted_script.h"
#include "third_party/blink/renderer/core/css/css_color_value.h"
#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/element_rare_data.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/event_listener.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/node_lists_node_data.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_checker.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/custom/custom_element.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_registry.h"
#include "third_party/blink/renderer/core/html/custom/element_internals.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/labels_node_list.h"
#include "third_party/blink/renderer/core/html/html_br_element.h"
#include "third_party/blink/renderer/core/html/html_dimension.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_template_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/adjust_for_absolute_zoom.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/mathml_names.h"
#include "third_party/blink/renderer/core/page/spatial_navigation.h"
#include "third_party/blink/renderer/core/svg/svg_svg_element.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_script.h"
#include "third_party/blink/renderer/core/xml_names.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/language.h"
#include "third_party/blink/renderer/platform/text/bidi_resolver.h"
#include "third_party/blink/renderer/platform/text/bidi_text_run.h"
#include "third_party/blink/renderer/platform/text/text_run_iterator.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

using namespace cssvalue;
using namespace html_names;

using AttributeChangedFunction =
    void (HTMLElement::*)(const Element::AttributeModificationParams& params);

struct AttributeTriggers {
  const QualifiedName& attribute;
  WebFeature web_feature;
  const AtomicString& event;
  AttributeChangedFunction function;
};

namespace {

// https://w3c.github.io/editing/execCommand.html#editing-host
bool IsEditingHost(const Node& node) {
  auto* html_element = DynamicTo<HTMLElement>(node);
  if (!html_element)
    return false;
  String normalized_value = html_element->contentEditable();
  if (normalized_value == "true" || normalized_value == "plaintext-only")
    return true;
  return node.GetDocument().InDesignMode() &&
         node.GetDocument().documentElement() == &node;
}

// https://w3c.github.io/editing/execCommand.html#editable
bool IsEditable(const Node& node) {
  if (IsEditingHost(node))
    return false;
  auto* html_element = DynamicTo<HTMLElement>(node);
  if (html_element && html_element->contentEditable() == "false")
    return false;
  if (!node.parentNode())
    return false;
  if (!IsEditingHost(*node.parentNode()) && !IsEditable(*node.parentNode()))
    return false;
  if (html_element)
    return true;
  if (IsSVGSVGElement(node))
    return true;
  auto* element = DynamicTo<Element>(node);
  if (element && element->HasTagName(mathml_names::kMathTag))
    return true;
  return !element && node.parentNode()->IsHTMLElement();
}

const WebFeature kNoWebFeature = static_cast<WebFeature>(0);

}  // anonymous namespace

String HTMLElement::DebugNodeName() const {
  if (GetDocument().IsHTMLDocument()) {
    return TagQName().HasPrefix() ? Element::nodeName().UpperASCII()
                                  : TagQName().LocalName().UpperASCII();
  }
  return Element::nodeName();
}

String HTMLElement::nodeName() const {
  // localNameUpper may intern and cache an AtomicString.
  DCHECK(IsMainThread());

  // FIXME: Would be nice to have an atomicstring lookup based off uppercase
  // chars that does not have to copy the string on a hit in the hash.
  // FIXME: We should have a way to detect XHTML elements and replace the
  // hasPrefix() check with it.
  if (GetDocument().IsHTMLDocument()) {
    if (!TagQName().HasPrefix())
      return TagQName().LocalNameUpper();
    return Element::nodeName().UpperASCII();
  }
  return Element::nodeName();
}

bool HTMLElement::ShouldSerializeEndTag() const {
  // See https://www.w3.org/TR/DOM-Parsing/
  if (HasTagName(kAreaTag) || HasTagName(kBaseTag) ||
      HasTagName(kBasefontTag) || HasTagName(kBgsoundTag) ||
      HasTagName(kBrTag) || HasTagName(kColTag) || HasTagName(kEmbedTag) ||
      HasTagName(kFrameTag) || HasTagName(kHrTag) || HasTagName(kImgTag) ||
      HasTagName(kInputTag) || HasTagName(kKeygenTag) || HasTagName(kLinkTag) ||
      HasTagName(kMetaTag) || HasTagName(kParamTag) || HasTagName(kSourceTag) ||
      HasTagName(kTrackTag) || HasTagName(kWbrTag))
    return false;
  return true;
}

static inline CSSValueID UnicodeBidiAttributeForDirAuto(HTMLElement* element) {
  if (element->HasTagName(kPreTag) || element->HasTagName(kTextareaTag))
    return CSSValueID::kWebkitPlaintext;
  // FIXME: For bdo element, dir="auto" should result in "bidi-override isolate"
  // but we don't support having multiple values in unicode-bidi yet.
  // See https://bugs.webkit.org/show_bug.cgi?id=73164.
  return CSSValueID::kWebkitIsolate;
}

unsigned HTMLElement::ParseBorderWidthAttribute(
    const AtomicString& value) const {
  unsigned border_width = 0;
  if (value.IsEmpty() || !ParseHTMLNonNegativeInteger(value, border_width)) {
    if (HasTagName(kTableTag) && !value.IsNull())
      return 1;
  }
  return border_width;
}

void HTMLElement::ApplyBorderAttributeToStyle(
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kBorderWidth,
                                          ParseBorderWidthAttribute(value),
                                          CSSPrimitiveValue::UnitType::kPixels);
  AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kBorderStyle,
                                          CSSValueID::kSolid);
}

void HTMLElement::MapLanguageAttributeToLocale(
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  if (!value.IsEmpty()) {
    // Have to quote so the locale id is treated as a string instead of as a CSS
    // keyword.
    AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kWebkitLocale,
                                            SerializeString(value));

    // FIXME: Remove the following UseCounter code when we collect enough
    // data.
    UseCounter::Count(GetDocument(), WebFeature::kLangAttribute);
    if (IsHTMLHtmlElement(*this))
      UseCounter::Count(GetDocument(), WebFeature::kLangAttributeOnHTML);
    else if (IsHTMLBodyElement(*this))
      UseCounter::Count(GetDocument(), WebFeature::kLangAttributeOnBody);
    String html_language = value.GetString();
    wtf_size_t first_separator = html_language.find('-');
    if (first_separator != kNotFound)
      html_language = html_language.Left(first_separator);
    String ui_language = DefaultLanguage();
    first_separator = ui_language.find('-');
    if (first_separator != kNotFound)
      ui_language = ui_language.Left(first_separator);
    first_separator = ui_language.find('_');
    if (first_separator != kNotFound)
      ui_language = ui_language.Left(first_separator);
    if (!DeprecatedEqualIgnoringCase(html_language, ui_language)) {
      UseCounter::Count(GetDocument(),
                        WebFeature::kLangAttributeDoesNotMatchToUILocale);
    }
  } else {
    // The empty string means the language is explicitly unknown.
    AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kWebkitLocale,
                                            CSSValueID::kAuto);
  }
}

bool HTMLElement::IsPresentationAttribute(const QualifiedName& name) const {
  if (name == kAlignAttr || name == kContenteditableAttr ||
      name == kHiddenAttr || name == kLangAttr ||
      name.Matches(xml_names::kLangAttr) || name == kDraggableAttr ||
      name == kDirAttr)
    return true;
  return Element::IsPresentationAttribute(name);
}

static inline bool IsValidDirAttribute(const AtomicString& value) {
  return DeprecatedEqualIgnoringCase(value, "auto") ||
         DeprecatedEqualIgnoringCase(value, "ltr") ||
         DeprecatedEqualIgnoringCase(value, "rtl");
}

void HTMLElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  if (name == kAlignAttr) {
    if (DeprecatedEqualIgnoringCase(value, "middle")) {
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kTextAlign,
                                              CSSValueID::kCenter);
    } else {
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kTextAlign,
                                              value);
    }
  } else if (name == kContenteditableAttr) {
    if (value.IsEmpty() || DeprecatedEqualIgnoringCase(value, "true")) {
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kWebkitUserModify, CSSValueID::kReadWrite);
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kOverflowWrap, CSSValueID::kBreakWord);
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kWebkitLineBreak, CSSValueID::kAfterWhiteSpace);
      UseCounter::Count(GetDocument(), WebFeature::kContentEditableTrue);
      if (HasTagName(kHTMLTag)) {
        UseCounter::Count(GetDocument(),
                          WebFeature::kContentEditableTrueOnHTML);
      }
    } else if (DeprecatedEqualIgnoringCase(value, "plaintext-only")) {
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kWebkitUserModify,
          CSSValueID::kReadWritePlaintextOnly);
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kOverflowWrap, CSSValueID::kBreakWord);
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kWebkitLineBreak, CSSValueID::kAfterWhiteSpace);
      UseCounter::Count(GetDocument(),
                        WebFeature::kContentEditablePlainTextOnly);
    } else if (DeprecatedEqualIgnoringCase(value, "false")) {
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kWebkitUserModify, CSSValueID::kReadOnly);
    }
  } else if (name == kHiddenAttr) {
    AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kDisplay,
                                            CSSValueID::kNone);
  } else if (name == kDraggableAttr) {
    UseCounter::Count(GetDocument(), WebFeature::kDraggableAttribute);
    if (DeprecatedEqualIgnoringCase(value, "true")) {
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kWebkitUserDrag, CSSValueID::kElement);
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kUserSelect,
                                              CSSValueID::kNone);
    } else if (DeprecatedEqualIgnoringCase(value, "false")) {
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kWebkitUserDrag, CSSValueID::kNone);
    }
  } else if (name == kDirAttr) {
    if (DeprecatedEqualIgnoringCase(value, "auto")) {
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kUnicodeBidi,
          UnicodeBidiAttributeForDirAuto(this));
    } else {
      if (IsValidDirAttribute(value)) {
        AddPropertyToPresentationAttributeStyle(
            style, CSSPropertyID::kDirection, value);
      } else if (IsHTMLBodyElement(*this)) {
        AddPropertyToPresentationAttributeStyle(
            style, CSSPropertyID::kDirection, "ltr");
      }
      if (!HasTagName(kBdiTag) && !HasTagName(kBdoTag) &&
          !HasTagName(kOutputTag)) {
        AddPropertyToPresentationAttributeStyle(
            style, CSSPropertyID::kUnicodeBidi, CSSValueID::kIsolate);
      }
    }
  } else if (name.Matches(xml_names::kLangAttr)) {
    MapLanguageAttributeToLocale(value, style);
  } else if (name == kLangAttr) {
    // xml:lang has a higher priority than lang.
    if (!FastHasAttribute(xml_names::kLangAttr))
      MapLanguageAttributeToLocale(value, style);
  } else {
    Element::CollectStyleForPresentationAttribute(name, value, style);
  }
}

// static
AttributeTriggers* HTMLElement::TriggersForAttributeName(
    const QualifiedName& attr_name) {
  const AtomicString& kNoEvent = g_null_atom;
  static AttributeTriggers attribute_triggers[] = {
      {kDirAttr, kNoWebFeature, kNoEvent, &HTMLElement::OnDirAttrChanged},
      {kFormAttr, kNoWebFeature, kNoEvent, &HTMLElement::OnFormAttrChanged},
      {kInertAttr, WebFeature::kInertAttribute, kNoEvent,
       &HTMLElement::OnInertAttrChanged},
      {kLangAttr, kNoWebFeature, kNoEvent, &HTMLElement::OnLangAttrChanged},
      {kNonceAttr, kNoWebFeature, kNoEvent, &HTMLElement::OnNonceAttrChanged},
      {kTabindexAttr, kNoWebFeature, kNoEvent,
       &HTMLElement::OnTabIndexAttrChanged},
      {xml_names::kLangAttr, kNoWebFeature, kNoEvent,
       &HTMLElement::OnXMLLangAttrChanged},

      {kOnabortAttr, kNoWebFeature, event_type_names::kAbort, nullptr},
      {kOnactivateinvisibleAttr, kNoWebFeature,
       event_type_names::kActivateinvisible, nullptr},
      {kOnanimationendAttr, kNoWebFeature, event_type_names::kAnimationend,
       nullptr},
      {kOnanimationiterationAttr, kNoWebFeature,
       event_type_names::kAnimationiteration, nullptr},
      {kOnanimationstartAttr, kNoWebFeature, event_type_names::kAnimationstart,
       nullptr},
      {kOnauxclickAttr, kNoWebFeature, event_type_names::kAuxclick, nullptr},
      {kOnbeforeactivateAttr, kNoWebFeature, event_type_names::kBeforeactivate,
       nullptr},
      {kOnbeforecopyAttr, kNoWebFeature, event_type_names::kBeforecopy,
       nullptr},
      {kOnbeforecutAttr, kNoWebFeature, event_type_names::kBeforecut, nullptr},
      {kOnbeforepasteAttr, kNoWebFeature, event_type_names::kBeforepaste,
       nullptr},
      {kOnblurAttr, kNoWebFeature, event_type_names::kBlur, nullptr},
      {kOncancelAttr, kNoWebFeature, event_type_names::kCancel, nullptr},
      {kOncanplayAttr, kNoWebFeature, event_type_names::kCanplay, nullptr},
      {kOncanplaythroughAttr, kNoWebFeature, event_type_names::kCanplaythrough,
       nullptr},
      {kOnchangeAttr, kNoWebFeature, event_type_names::kChange, nullptr},
      {kOnclickAttr, kNoWebFeature, event_type_names::kClick, nullptr},
      {kOncloseAttr, kNoWebFeature, event_type_names::kClose, nullptr},
      {kOncontextmenuAttr, kNoWebFeature, event_type_names::kContextmenu,
       nullptr},
      {kOncopyAttr, kNoWebFeature, event_type_names::kCopy, nullptr},
      {kOncuechangeAttr, kNoWebFeature, event_type_names::kCuechange, nullptr},
      {kOncutAttr, kNoWebFeature, event_type_names::kCut, nullptr},
      {kOndblclickAttr, kNoWebFeature, event_type_names::kDblclick, nullptr},
      {kOndragAttr, kNoWebFeature, event_type_names::kDrag, nullptr},
      {kOndragendAttr, kNoWebFeature, event_type_names::kDragend, nullptr},
      {kOndragenterAttr, kNoWebFeature, event_type_names::kDragenter, nullptr},
      {kOndragleaveAttr, kNoWebFeature, event_type_names::kDragleave, nullptr},
      {kOndragoverAttr, kNoWebFeature, event_type_names::kDragover, nullptr},
      {kOndragstartAttr, kNoWebFeature, event_type_names::kDragstart, nullptr},
      {kOndropAttr, kNoWebFeature, event_type_names::kDrop, nullptr},
      {kOndurationchangeAttr, kNoWebFeature, event_type_names::kDurationchange,
       nullptr},
      {kOnemptiedAttr, kNoWebFeature, event_type_names::kEmptied, nullptr},
      {kOnendedAttr, kNoWebFeature, event_type_names::kEnded, nullptr},
      {kOnerrorAttr, kNoWebFeature, event_type_names::kError, nullptr},
      {kOnfocusAttr, kNoWebFeature, event_type_names::kFocus, nullptr},
      {kOnfocusinAttr, kNoWebFeature, event_type_names::kFocusin, nullptr},
      {kOnfocusoutAttr, kNoWebFeature, event_type_names::kFocusout, nullptr},
      {kOnformdataAttr, kNoWebFeature, event_type_names::kFormdata, nullptr},
      {kOngotpointercaptureAttr, kNoWebFeature,
       event_type_names::kGotpointercapture, nullptr},
      {kOninputAttr, kNoWebFeature, event_type_names::kInput, nullptr},
      {kOninvalidAttr, kNoWebFeature, event_type_names::kInvalid, nullptr},
      {kOnkeydownAttr, kNoWebFeature, event_type_names::kKeydown, nullptr},
      {kOnkeypressAttr, kNoWebFeature, event_type_names::kKeypress, nullptr},
      {kOnkeyupAttr, kNoWebFeature, event_type_names::kKeyup, nullptr},
      {kOnloadAttr, kNoWebFeature, event_type_names::kLoad, nullptr},
      {kOnloadeddataAttr, kNoWebFeature, event_type_names::kLoadeddata,
       nullptr},
      {kOnloadedmetadataAttr, kNoWebFeature, event_type_names::kLoadedmetadata,
       nullptr},
      {kOnloadstartAttr, kNoWebFeature, event_type_names::kLoadstart, nullptr},
      {kOnlostpointercaptureAttr, kNoWebFeature,
       event_type_names::kLostpointercapture, nullptr},
      {kOnmousedownAttr, kNoWebFeature, event_type_names::kMousedown, nullptr},
      {kOnmouseenterAttr, kNoWebFeature, event_type_names::kMouseenter,
       nullptr},
      {kOnmouseleaveAttr, kNoWebFeature, event_type_names::kMouseleave,
       nullptr},
      {kOnmousemoveAttr, kNoWebFeature, event_type_names::kMousemove, nullptr},
      {kOnmouseoutAttr, kNoWebFeature, event_type_names::kMouseout, nullptr},
      {kOnmouseoverAttr, kNoWebFeature, event_type_names::kMouseover, nullptr},
      {kOnmouseupAttr, kNoWebFeature, event_type_names::kMouseup, nullptr},
      {kOnmousewheelAttr, kNoWebFeature, event_type_names::kMousewheel,
       nullptr},
      {kOnoverscrollAttr, kNoWebFeature, event_type_names::kOverscroll,
       nullptr},
      {kOnpasteAttr, kNoWebFeature, event_type_names::kPaste, nullptr},
      {kOnpauseAttr, kNoWebFeature, event_type_names::kPause, nullptr},
      {kOnplayAttr, kNoWebFeature, event_type_names::kPlay, nullptr},
      {kOnplayingAttr, kNoWebFeature, event_type_names::kPlaying, nullptr},
      {kOnpointercancelAttr, kNoWebFeature, event_type_names::kPointercancel,
       nullptr},
      {kOnpointerdownAttr, kNoWebFeature, event_type_names::kPointerdown,
       nullptr},
      {kOnpointerenterAttr, kNoWebFeature, event_type_names::kPointerenter,
       nullptr},
      {kOnpointerleaveAttr, kNoWebFeature, event_type_names::kPointerleave,
       nullptr},
      {kOnpointermoveAttr, kNoWebFeature, event_type_names::kPointermove,
       nullptr},
      {kOnpointeroutAttr, kNoWebFeature, event_type_names::kPointerout,
       nullptr},
      {kOnpointeroverAttr, kNoWebFeature, event_type_names::kPointerover,
       nullptr},
      {kOnpointerrawupdateAttr, kNoWebFeature,
       event_type_names::kPointerrawupdate, nullptr},
      {kOnpointerupAttr, kNoWebFeature, event_type_names::kPointerup, nullptr},
      {kOnprogressAttr, kNoWebFeature, event_type_names::kProgress, nullptr},
      {kOnratechangeAttr, kNoWebFeature, event_type_names::kRatechange,
       nullptr},
      {kOnresetAttr, kNoWebFeature, event_type_names::kReset, nullptr},
      {kOnresizeAttr, kNoWebFeature, event_type_names::kResize, nullptr},
      {kOnscrollAttr, kNoWebFeature, event_type_names::kScroll, nullptr},
      {kOnscrollendAttr, kNoWebFeature, event_type_names::kScrollend, nullptr},
      {kOnseekedAttr, kNoWebFeature, event_type_names::kSeeked, nullptr},
      {kOnseekingAttr, kNoWebFeature, event_type_names::kSeeking, nullptr},
      {kOnselectAttr, kNoWebFeature, event_type_names::kSelect, nullptr},
      {kOnselectstartAttr, kNoWebFeature, event_type_names::kSelectstart,
       nullptr},
      {kOnstalledAttr, kNoWebFeature, event_type_names::kStalled, nullptr},
      {kOnsubmitAttr, kNoWebFeature, event_type_names::kSubmit, nullptr},
      {kOnsuspendAttr, kNoWebFeature, event_type_names::kSuspend, nullptr},
      {kOntimeupdateAttr, kNoWebFeature, event_type_names::kTimeupdate,
       nullptr},
      {kOntoggleAttr, kNoWebFeature, event_type_names::kToggle, nullptr},
      {kOntouchcancelAttr, kNoWebFeature, event_type_names::kTouchcancel,
       nullptr},
      {kOntouchendAttr, kNoWebFeature, event_type_names::kTouchend, nullptr},
      {kOntouchmoveAttr, kNoWebFeature, event_type_names::kTouchmove, nullptr},
      {kOntouchstartAttr, kNoWebFeature, event_type_names::kTouchstart,
       nullptr},
      {kOntransitionendAttr, kNoWebFeature,
       event_type_names::kWebkitTransitionEnd, nullptr},
      {kOnvolumechangeAttr, kNoWebFeature, event_type_names::kVolumechange,
       nullptr},
      {kOnwaitingAttr, kNoWebFeature, event_type_names::kWaiting, nullptr},
      {kOnwebkitanimationendAttr, kNoWebFeature,
       event_type_names::kWebkitAnimationEnd, nullptr},
      {kOnwebkitanimationiterationAttr, kNoWebFeature,
       event_type_names::kWebkitAnimationIteration, nullptr},
      {kOnwebkitanimationstartAttr, kNoWebFeature,
       event_type_names::kWebkitAnimationStart, nullptr},
      {kOnwebkitfullscreenchangeAttr, kNoWebFeature,
       event_type_names::kWebkitfullscreenchange, nullptr},
      {kOnwebkitfullscreenerrorAttr, kNoWebFeature,
       event_type_names::kWebkitfullscreenerror, nullptr},
      {kOnwebkittransitionendAttr, kNoWebFeature,
       event_type_names::kWebkitTransitionEnd, nullptr},
      {kOnwheelAttr, kNoWebFeature, event_type_names::kWheel, nullptr},

      {kAriaActivedescendantAttr, WebFeature::kARIAActiveDescendantAttribute,
       kNoEvent, nullptr},
      {kAriaAtomicAttr, WebFeature::kARIAAtomicAttribute, kNoEvent, nullptr},
      {kAriaAutocompleteAttr, WebFeature::kARIAAutocompleteAttribute, kNoEvent,
       nullptr},
      {kAriaBusyAttr, WebFeature::kARIABusyAttribute, kNoEvent, nullptr},
      {kAriaCheckedAttr, WebFeature::kARIACheckedAttribute, kNoEvent, nullptr},
      {kAriaColcountAttr, WebFeature::kARIAColCountAttribute, kNoEvent,
       nullptr},
      {kAriaColindexAttr, WebFeature::kARIAColIndexAttribute, kNoEvent,
       nullptr},
      {kAriaColspanAttr, WebFeature::kARIAColSpanAttribute, kNoEvent, nullptr},
      {kAriaControlsAttr, WebFeature::kARIAControlsAttribute, kNoEvent,
       nullptr},
      {kAriaCurrentAttr, WebFeature::kARIACurrentAttribute, kNoEvent, nullptr},
      {kAriaDescribedbyAttr, WebFeature::kARIADescribedByAttribute, kNoEvent,
       nullptr},
      {kAriaDetailsAttr, WebFeature::kARIADetailsAttribute, kNoEvent, nullptr},
      {kAriaDisabledAttr, WebFeature::kARIADisabledAttribute, kNoEvent,
       nullptr},
      {kAriaDropeffectAttr, WebFeature::kARIADropEffectAttribute, kNoEvent,
       nullptr},
      {kAriaErrormessageAttr, WebFeature::kARIAErrorMessageAttribute, kNoEvent,
       nullptr},
      {kAriaExpandedAttr, WebFeature::kARIAExpandedAttribute, kNoEvent,
       nullptr},
      {kAriaFlowtoAttr, WebFeature::kARIAFlowToAttribute, kNoEvent, nullptr},
      {kAriaGrabbedAttr, WebFeature::kARIAGrabbedAttribute, kNoEvent, nullptr},
      {kAriaHaspopupAttr, WebFeature::kARIAHasPopupAttribute, kNoEvent,
       nullptr},
      {kAriaHelpAttr, WebFeature::kARIAHelpAttribute, kNoEvent, nullptr},
      {kAriaHiddenAttr, WebFeature::kARIAHiddenAttribute, kNoEvent, nullptr},
      {kAriaInvalidAttr, WebFeature::kARIAInvalidAttribute, kNoEvent, nullptr},
      {kAriaKeyshortcutsAttr, WebFeature::kARIAKeyShortcutsAttribute, kNoEvent,
       nullptr},
      {kAriaLabelAttr, WebFeature::kARIALabelAttribute, kNoEvent, nullptr},
      {kAriaLabeledbyAttr, WebFeature::kARIALabeledByAttribute, kNoEvent,
       nullptr},
      {kAriaLabelledbyAttr, WebFeature::kARIALabelledByAttribute, kNoEvent,
       nullptr},
      {kAriaLevelAttr, WebFeature::kARIALevelAttribute, kNoEvent, nullptr},
      {kAriaLiveAttr, WebFeature::kARIALiveAttribute, kNoEvent, nullptr},
      {kAriaModalAttr, WebFeature::kARIAModalAttribute, kNoEvent, nullptr},
      {kAriaMultilineAttr, WebFeature::kARIAMultilineAttribute, kNoEvent,
       nullptr},
      {kAriaMultiselectableAttr, WebFeature::kARIAMultiselectableAttribute,
       kNoEvent, nullptr},
      {kAriaOrientationAttr, WebFeature::kARIAOrientationAttribute, kNoEvent,
       nullptr},
      {kAriaOwnsAttr, WebFeature::kARIAOwnsAttribute, kNoEvent, nullptr},
      {kAriaPlaceholderAttr, WebFeature::kARIAPlaceholderAttribute, kNoEvent,
       nullptr},
      {kAriaPosinsetAttr, WebFeature::kARIAPosInSetAttribute, kNoEvent,
       nullptr},
      {kAriaPressedAttr, WebFeature::kARIAPressedAttribute, kNoEvent, nullptr},
      {kAriaReadonlyAttr, WebFeature::kARIAReadOnlyAttribute, kNoEvent,
       nullptr},
      {kAriaRelevantAttr, WebFeature::kARIARelevantAttribute, kNoEvent,
       nullptr},
      {kAriaRequiredAttr, WebFeature::kARIARequiredAttribute, kNoEvent,
       nullptr},
      {kAriaRoledescriptionAttr, WebFeature::kARIARoleDescriptionAttribute,
       kNoEvent, nullptr},
      {kAriaRowcountAttr, WebFeature::kARIARowCountAttribute, kNoEvent,
       nullptr},
      {kAriaRowindexAttr, WebFeature::kARIARowIndexAttribute, kNoEvent,
       nullptr},
      {kAriaRowspanAttr, WebFeature::kARIARowSpanAttribute, kNoEvent, nullptr},
      {kAriaSelectedAttr, WebFeature::kARIASelectedAttribute, kNoEvent,
       nullptr},
      {kAriaSetsizeAttr, WebFeature::kARIASetSizeAttribute, kNoEvent, nullptr},
      {kAriaSortAttr, WebFeature::kARIASortAttribute, kNoEvent, nullptr},
      {kAriaValuemaxAttr, WebFeature::kARIAValueMaxAttribute, kNoEvent,
       nullptr},
      {kAriaValueminAttr, WebFeature::kARIAValueMinAttribute, kNoEvent,
       nullptr},
      {kAriaValuenowAttr, WebFeature::kARIAValueNowAttribute, kNoEvent,
       nullptr},
      {kAriaValuetextAttr, WebFeature::kARIAValueTextAttribute, kNoEvent,
       nullptr},
      {kAutocapitalizeAttr, WebFeature::kAutocapitalizeAttribute, kNoEvent,
       nullptr},
  };

  using AttributeToTriggerIndexMap = HashMap<QualifiedName, uint32_t>;
  DEFINE_STATIC_LOCAL(AttributeToTriggerIndexMap,
                      attribute_to_trigger_index_map, ());
  if (!attribute_to_trigger_index_map.size()) {
    for (uint32_t i = 0; i < base::size(attribute_triggers); ++i)
      attribute_to_trigger_index_map.insert(attribute_triggers[i].attribute, i);
  }

  auto iter = attribute_to_trigger_index_map.find(attr_name);
  if (iter != attribute_to_trigger_index_map.end())
    return &attribute_triggers[iter->value];
  return nullptr;
}

// static
const AtomicString& HTMLElement::EventNameForAttributeName(
    const QualifiedName& attr_name) {
  AttributeTriggers* triggers = TriggersForAttributeName(attr_name);
  if (triggers)
    return triggers->event;
  return g_null_atom;
}

void HTMLElement::AttributeChanged(const AttributeModificationParams& params) {
  Element::AttributeChanged(params);
  if (params.name == html_names::kDisabledAttr &&
      params.old_value.IsNull() != params.new_value.IsNull()) {
    if (IsFormAssociatedCustomElement()) {
      EnsureElementInternals().DisabledAttributeChanged();
      if (params.reason == AttributeModificationReason::kDirectly &&
          IsDisabledFormControl() &&
          AdjustedFocusedElementInTreeScope() == this)
        blur();
    }
    return;
  }
  if (params.reason != AttributeModificationReason::kDirectly)
    return;
  // adjustedFocusedElementInTreeScope() is not trivial. We should check
  // attribute names, then call adjustedFocusedElementInTreeScope().
  if (params.name == kHiddenAttr && !params.new_value.IsNull()) {
    if (AdjustedFocusedElementInTreeScope() == this)
      blur();
  } else if (params.name == kContenteditableAttr) {
    if (GetDocument().GetFrame()) {
      GetDocument()
          .GetFrame()
          ->GetSpellChecker()
          .RemoveSpellingAndGrammarMarkers(
              *this, SpellChecker::ElementsType::kOnlyNonEditable);
    }
    if (AdjustedFocusedElementInTreeScope() != this)
      return;
    // The attribute change may cause supportsFocus() to return false
    // for the element which had focus.
    //
    // TODO(tkent): We should avoid updating style.  We'd like to check only
    // DOM-level focusability here.
    GetDocument().UpdateStyleAndLayoutTreeForNode(this);
    if (!SupportsFocus())
      blur();
  }
}

void HTMLElement::ParseAttribute(const AttributeModificationParams& params) {
  AttributeTriggers* triggers = TriggersForAttributeName(params.name);
  if (!triggers)
    return;

  if (triggers->event != g_null_atom) {
    SetAttributeEventListener(
        triggers->event,
        CreateAttributeEventListener(this, params.name, params.new_value));
  }

  if (triggers->web_feature != kNoWebFeature) {
    // Count usage of attributes but ignore attributes in user agent shadow DOM.
    if (!IsInUserAgentShadowRoot()) {
      UseCounter::Count(GetDocument(), triggers->web_feature);
    }
  }
  if (triggers->function)
    ((*this).*(triggers->function))(params);
}

DocumentFragment* HTMLElement::TextToFragment(const String& text,
                                              ExceptionState& exception_state) {
  DocumentFragment* fragment = DocumentFragment::Create(GetDocument());
  unsigned i, length = text.length();
  UChar c = 0;
  for (unsigned start = 0; start < length;) {
    // Find next line break.
    for (i = start; i < length; i++) {
      c = text[i];
      if (c == '\r' || c == '\n')
        break;
    }

    if (i > start) {
      fragment->AppendChild(
          Text::Create(GetDocument(), text.Substring(start, i - start)),
          exception_state);
      if (exception_state.HadException())
        return nullptr;
    }

    if (i == length)
      break;

    fragment->AppendChild(MakeGarbageCollected<HTMLBRElement>(GetDocument()),
                          exception_state);
    if (exception_state.HadException())
      return nullptr;

    // Make sure \r\n doesn't result in two line breaks.
    if (c == '\r' && i + 1 < length && text[i + 1] == '\n')
      i++;

    start = i + 1;  // Character after line break.
  }

  return fragment;
}

void HTMLElement::setInnerText(
    const StringOrTrustedScript& string_or_trusted_script,
    ExceptionState& exception_state) {
  String value;
  if (string_or_trusted_script.IsString())
    value = string_or_trusted_script.GetAsString();
  else if (string_or_trusted_script.IsTrustedScript())
    value = string_or_trusted_script.GetAsTrustedScript()->toString();
  setInnerText(value, exception_state);
}

void HTMLElement::setInnerText(
    const StringTreatNullAsEmptyStringOrTrustedScript& string_or_trusted_script,
    ExceptionState& exception_state) {
  StringOrTrustedScript tmp;
  if (string_or_trusted_script.IsString())
    tmp.SetString(string_or_trusted_script.GetAsString());
  else if (string_or_trusted_script.IsTrustedScript())
    tmp.SetTrustedScript(string_or_trusted_script.GetAsTrustedScript());
  setInnerText(tmp, exception_state);
}

void HTMLElement::innerText(
    StringTreatNullAsEmptyStringOrTrustedScript& result) {
  result.SetString(innerText());
}

void HTMLElement::innerText(StringOrTrustedScript& result) {
  result.SetString(innerText());
}

String HTMLElement::innerText() {
  return Element::innerText();
}

void HTMLElement::setInnerText(const String& text,
                               ExceptionState& exception_state) {
  // FIXME: This doesn't take whitespace collapsing into account at all.

  if (!text.Contains('\n') && !text.Contains('\r')) {
    if (text.IsEmpty()) {
      RemoveChildren();
      return;
    }
    ReplaceChildrenWithText(this, text, exception_state);
    return;
  }

  // Add text nodes and <br> elements.
  DocumentFragment* fragment = TextToFragment(text, exception_state);
  if (!exception_state.HadException())
    ReplaceChildrenWithFragment(this, fragment, exception_state);
}

void HTMLElement::setOuterText(const String& text,
                               ExceptionState& exception_state) {
  ContainerNode* parent = parentNode();
  if (!parent) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNoModificationAllowedError,
        "The element has no parent.");
    return;
  }

  Node* prev = previousSibling();
  Node* next = nextSibling();
  Node* new_child = nullptr;

  // Convert text to fragment with <br> tags instead of linebreaks if needed.
  if (text.Contains('\r') || text.Contains('\n'))
    new_child = TextToFragment(text, exception_state);
  else
    new_child = Text::Create(GetDocument(), text);

  // textToFragment might cause mutation events.
  if (!parentNode())
    exception_state.ThrowDOMException(DOMExceptionCode::kHierarchyRequestError,
                                      "The element has no parent.");

  if (exception_state.HadException())
    return;

  parent->ReplaceChild(new_child, this, exception_state);

  Node* node = next ? next->previousSibling() : nullptr;
  auto* next_text_node = DynamicTo<Text>(node);
  if (!exception_state.HadException() && next_text_node)
    MergeWithNextTextNode(next_text_node, exception_state);

  auto* prev_text_node = DynamicTo<Text>(prev);
  if (!exception_state.HadException() && prev && prev->IsTextNode())
    MergeWithNextTextNode(prev_text_node, exception_state);
}

void HTMLElement::ApplyAlignmentAttributeToStyle(
    const AtomicString& alignment,
    MutableCSSPropertyValueSet* style) {
  // Vertical alignment with respect to the current baseline of the text
  // right or left means floating images.
  CSSValueID float_value = CSSValueID::kInvalid;
  CSSValueID vertical_align_value = CSSValueID::kInvalid;

  if (DeprecatedEqualIgnoringCase(alignment, "absmiddle")) {
    vertical_align_value = CSSValueID::kMiddle;
  } else if (DeprecatedEqualIgnoringCase(alignment, "absbottom")) {
    vertical_align_value = CSSValueID::kBottom;
  } else if (DeprecatedEqualIgnoringCase(alignment, "left")) {
    float_value = CSSValueID::kLeft;
    vertical_align_value = CSSValueID::kTop;
  } else if (DeprecatedEqualIgnoringCase(alignment, "right")) {
    float_value = CSSValueID::kRight;
    vertical_align_value = CSSValueID::kTop;
  } else if (DeprecatedEqualIgnoringCase(alignment, "top")) {
    vertical_align_value = CSSValueID::kTop;
  } else if (DeprecatedEqualIgnoringCase(alignment, "middle")) {
    vertical_align_value = CSSValueID::kWebkitBaselineMiddle;
  } else if (DeprecatedEqualIgnoringCase(alignment, "center")) {
    vertical_align_value = CSSValueID::kMiddle;
  } else if (DeprecatedEqualIgnoringCase(alignment, "bottom")) {
    vertical_align_value = CSSValueID::kBaseline;
  } else if (DeprecatedEqualIgnoringCase(alignment, "texttop")) {
    vertical_align_value = CSSValueID::kTextTop;
  }

  if (IsValidCSSValueID(float_value)) {
    AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kFloat,
                                            float_value);
  }

  if (IsValidCSSValueID(vertical_align_value)) {
    AddPropertyToPresentationAttributeStyle(
        style, CSSPropertyID::kVerticalAlign, vertical_align_value);
  }
}

bool HTMLElement::HasCustomFocusLogic() const {
  return false;
}

String HTMLElement::contentEditable() const {
  const AtomicString& value = FastGetAttribute(kContenteditableAttr);

  if (value.IsNull())
    return "inherit";
  if (value.IsEmpty() || DeprecatedEqualIgnoringCase(value, "true"))
    return "true";
  if (DeprecatedEqualIgnoringCase(value, "false"))
    return "false";
  if (DeprecatedEqualIgnoringCase(value, "plaintext-only"))
    return "plaintext-only";

  return "inherit";
}

void HTMLElement::setContentEditable(const String& enabled,
                                     ExceptionState& exception_state) {
  if (DeprecatedEqualIgnoringCase(enabled, "true"))
    setAttribute(kContenteditableAttr, "true");
  else if (DeprecatedEqualIgnoringCase(enabled, "false"))
    setAttribute(kContenteditableAttr, "false");
  else if (DeprecatedEqualIgnoringCase(enabled, "plaintext-only"))
    setAttribute(kContenteditableAttr, "plaintext-only");
  else if (DeprecatedEqualIgnoringCase(enabled, "inherit"))
    removeAttribute(kContenteditableAttr);
  else
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "The value provided ('" + enabled +
                                          "') is not one of 'true', 'false', "
                                          "'plaintext-only', or 'inherit'.");
}

const AtomicString& HTMLElement::autocapitalize() const {
  DEFINE_STATIC_LOCAL(const AtomicString, kOff, ("off"));
  DEFINE_STATIC_LOCAL(const AtomicString, kNone, ("none"));
  DEFINE_STATIC_LOCAL(const AtomicString, kCharacters, ("characters"));
  DEFINE_STATIC_LOCAL(const AtomicString, kWords, ("words"));
  DEFINE_STATIC_LOCAL(const AtomicString, kSentences, ("sentences"));

  const AtomicString& value = FastGetAttribute(kAutocapitalizeAttr);
  if (value.IsEmpty())
    return g_empty_atom;

  if (EqualIgnoringASCIICase(value, kNone) ||
      EqualIgnoringASCIICase(value, kOff))
    return kNone;
  if (EqualIgnoringASCIICase(value, kCharacters))
    return kCharacters;
  if (EqualIgnoringASCIICase(value, kWords))
    return kWords;
  // "sentences", "on", or an invalid value
  return kSentences;
}

void HTMLElement::setAutocapitalize(const AtomicString& value) {
  setAttribute(kAutocapitalizeAttr, value);
}

bool HTMLElement::isContentEditableForBinding() const {
  return IsEditingHost(*this) || IsEditable(*this);
}

bool HTMLElement::draggable() const {
  return DeprecatedEqualIgnoringCase(getAttribute(kDraggableAttr), "true");
}

void HTMLElement::setDraggable(bool value) {
  setAttribute(kDraggableAttr, value ? "true" : "false");
}

bool HTMLElement::spellcheck() const {
  return IsSpellCheckingEnabled();
}

void HTMLElement::setSpellcheck(bool enable) {
  setAttribute(kSpellcheckAttr, enable ? "true" : "false");
}

void HTMLElement::click() {
  DispatchSimulatedClick(nullptr, kSendNoEvents,
                         SimulatedClickCreationScope::kFromScript);
}

void HTMLElement::AccessKeyAction(bool send_mouse_events) {
  DispatchSimulatedClick(
      nullptr, send_mouse_events ? kSendMouseUpDownEvents : kSendNoEvents);
}

String HTMLElement::title() const {
  return FastGetAttribute(kTitleAttr);
}

int HTMLElement::tabIndex() const {
  if (SupportsFocus() ||
      (RuntimeEnabledFeatures::KeyboardFocusableScrollersEnabled() &&
       IsScrollableNode(this))) {
    return Element::tabIndex();
  }
  return -1;
}

TranslateAttributeMode HTMLElement::GetTranslateAttributeMode() const {
  const AtomicString& value = getAttribute(kTranslateAttr);

  if (value == g_null_atom)
    return kTranslateAttributeInherit;
  if (DeprecatedEqualIgnoringCase(value, "yes") ||
      DeprecatedEqualIgnoringCase(value, ""))
    return kTranslateAttributeYes;
  if (DeprecatedEqualIgnoringCase(value, "no"))
    return kTranslateAttributeNo;

  return kTranslateAttributeInherit;
}

bool HTMLElement::translate() const {
  for (const HTMLElement* element = this; element;
       element = Traversal<HTMLElement>::FirstAncestor(*element)) {
    TranslateAttributeMode mode = element->GetTranslateAttributeMode();
    if (mode != kTranslateAttributeInherit) {
      DCHECK(mode == kTranslateAttributeYes || mode == kTranslateAttributeNo);
      return mode == kTranslateAttributeYes;
    }
  }

  // Default on the root element is translate=yes.
  return true;
}

void HTMLElement::setTranslate(bool enable) {
  setAttribute(kTranslateAttr, enable ? "yes" : "no");
}

// Returns the conforming 'dir' value associated with the state the attribute is
// in (in its canonical case), if any, or the empty string if the attribute is
// in a state that has no associated keyword value or if the attribute is not in
// a defined state (e.g. the attribute is missing and there is no missing value
// default).
// http://www.whatwg.org/specs/web-apps/current-work/multipage/common-dom-interfaces.html#limited-to-only-known-values
static inline const AtomicString& ToValidDirValue(const AtomicString& value) {
  DEFINE_STATIC_LOCAL(const AtomicString, ltr_value, ("ltr"));
  DEFINE_STATIC_LOCAL(const AtomicString, rtl_value, ("rtl"));
  DEFINE_STATIC_LOCAL(const AtomicString, auto_value, ("auto"));

  if (DeprecatedEqualIgnoringCase(value, ltr_value))
    return ltr_value;
  if (DeprecatedEqualIgnoringCase(value, rtl_value))
    return rtl_value;
  if (DeprecatedEqualIgnoringCase(value, auto_value))
    return auto_value;
  return g_null_atom;
}

const AtomicString& HTMLElement::dir() {
  return ToValidDirValue(FastGetAttribute(kDirAttr));
}

void HTMLElement::setDir(const AtomicString& value) {
  setAttribute(kDirAttr, value);
}

HTMLFormElement* HTMLElement::FindFormAncestor() const {
  return Traversal<HTMLFormElement>::FirstAncestor(*this);
}

static inline bool ElementAffectsDirectionality(const Node* node) {
  auto* html_element = DynamicTo<HTMLElement>(node);
  return html_element && (IsHTMLBDIElement(*html_element) ||
                          html_element->hasAttribute(kDirAttr));
}

void HTMLElement::ChildrenChanged(const ChildrenChange& change) {
  Element::ChildrenChanged(change);
  AdjustDirectionalityIfNeededAfterChildrenChanged(change);
}

bool HTMLElement::HasDirectionAuto() const {
  // <bdi> defaults to dir="auto"
  // https://html.spec.whatwg.org/C/#the-bdi-element
  const AtomicString& direction = FastGetAttribute(kDirAttr);
  return (IsHTMLBDIElement(*this) && direction == g_null_atom) ||
         DeprecatedEqualIgnoringCase(direction, "auto");
}

TextDirection HTMLElement::DirectionalityIfhasDirAutoAttribute(
    bool& is_auto) const {
  is_auto = HasDirectionAuto();
  if (!is_auto)
    return TextDirection::kLtr;
  return Directionality();
}

TextDirection HTMLElement::Directionality() const {
  if (auto* input_element = ToHTMLInputElementOrNull(*this)) {
    bool has_strong_directionality;
    TextDirection text_direction = DetermineDirectionality(
        input_element->value(), &has_strong_directionality);
    return text_direction;
  }

  Node* node = FlatTreeTraversal::FirstChild(*this);
  while (node) {
    // Skip bdi, script, style and text form controls.
    auto* element = DynamicTo<Element>(node);
    if (DeprecatedEqualIgnoringCase(node->nodeName(), "bdi") ||
        IsHTMLScriptElement(*node) || IsHTMLStyleElement(*node) ||
        (element && element->IsTextControl()) ||
        (element && element->ShadowPseudoId() == "-webkit-input-placeholder")) {
      node = FlatTreeTraversal::NextSkippingChildren(*node, this);
      continue;
    }

    // Skip elements with valid dir attribute
    if (auto* element_node = DynamicTo<Element>(node)) {
      AtomicString dir_attribute_value =
          element_node->FastGetAttribute(kDirAttr);
      if (IsValidDirAttribute(dir_attribute_value)) {
        node = FlatTreeTraversal::NextSkippingChildren(*node, this);
        continue;
      }
    }

    if (node->IsTextNode()) {
      bool has_strong_directionality;
      TextDirection text_direction = DetermineDirectionality(
          node->textContent(true), &has_strong_directionality);
      if (has_strong_directionality)
        return text_direction;
    }
    node = FlatTreeTraversal::Next(*node, this);
  }
  return TextDirection::kLtr;
}

bool HTMLElement::SelfOrAncestorHasDirAutoAttribute() const {
  // TODO(esprehn): Storing this state in the computed style is bad, we
  // should be able to answer questions about the shape of the DOM and the
  // text contained inside it without having style.
  if (const ComputedStyle* style = GetComputedStyle())
    return style->SelfOrAncestorHasDirAutoAttribute();
  return false;
}

void HTMLElement::AdjustDirectionalityIfNeededAfterChildAttributeChanged(
    Element* child) {
  DCHECK(SelfOrAncestorHasDirAutoAttribute());
  const ComputedStyle* style = GetComputedStyle();
  if (style && style->Direction() != Directionality()) {
    for (Element* element_to_adjust = this; element_to_adjust;
         element_to_adjust =
             FlatTreeTraversal::ParentElement(*element_to_adjust)) {
      if (ElementAffectsDirectionality(element_to_adjust)) {
        element_to_adjust->SetNeedsStyleRecalc(
            kLocalStyleChange, StyleChangeReasonForTracing::Create(
                                   style_change_reason::kWritingModeChange));
        return;
      }
    }
  }
}

void HTMLElement::CalculateAndAdjustDirectionality() {
  TextDirection text_direction = Directionality();
  const ComputedStyle* style = GetComputedStyle();
  if (style && style->Direction() != text_direction) {
    SetNeedsStyleRecalc(kLocalStyleChange,
                        StyleChangeReasonForTracing::Create(
                            style_change_reason::kWritingModeChange));
  }
}

void HTMLElement::AdjustDirectionalityIfNeededAfterChildrenChanged(
    const ChildrenChange& change) {
  if (!SelfOrAncestorHasDirAutoAttribute())
    return;

  UpdateDistributionForFlatTreeTraversal();

  for (Element* element_to_adjust = this; element_to_adjust;
       element_to_adjust =
           FlatTreeTraversal::ParentElement(*element_to_adjust)) {
    if (ElementAffectsDirectionality(element_to_adjust)) {
      To<HTMLElement>(element_to_adjust)->CalculateAndAdjustDirectionality();
      return;
    }
  }
}

Node::InsertionNotificationRequest HTMLElement::InsertedInto(
    ContainerNode& insertion_point) {
  // Process the superclass first to ensure that `InActiveDocument()` is
  // updated.
  Element::InsertedInto(insertion_point);

  if (GetDocument().GetContentSecurityPolicy()->HasHeaderDeliveredPolicy() &&
      InActiveDocument() && FastHasAttribute(kNonceAttr)) {
    setAttribute(kNonceAttr, g_empty_atom);
  }
  if (IsFormAssociatedCustomElement())
    EnsureElementInternals().InsertedInto(insertion_point);

  return kInsertionDone;
}

void HTMLElement::RemovedFrom(ContainerNode& insertion_point) {
  Element::RemovedFrom(insertion_point);
  if (IsFormAssociatedCustomElement())
    EnsureElementInternals().RemovedFrom(insertion_point);
}

void HTMLElement::DidMoveToNewDocument(Document& old_document) {
  if (IsFormAssociatedCustomElement())
    EnsureElementInternals().DidMoveToNewDocument(old_document);
  Element::DidMoveToNewDocument(old_document);
}

void HTMLElement::AddHTMLLengthToStyle(MutableCSSPropertyValueSet* style,
                                       CSSPropertyID property_id,
                                       const String& value,
                                       AllowPercentage allow_percentage) {
  HTMLDimension dimension;
  if (!ParseDimensionValue(value, dimension))
    return;
  if (property_id == CSSPropertyID::kWidth &&
      (dimension.IsPercentage() || dimension.IsRelative())) {
    UseCounter::Count(GetDocument(), WebFeature::kHTMLElementDeprecatedWidth);
  }
  if (dimension.IsRelative())
    return;
  if (dimension.IsPercentage() && allow_percentage != kAllowPercentageValues)
    return;
  CSSPrimitiveValue::UnitType unit =
      dimension.IsPercentage() ? CSSPrimitiveValue::UnitType::kPercentage
                               : CSSPrimitiveValue::UnitType::kPixels;
  AddPropertyToPresentationAttributeStyle(style, property_id, dimension.Value(),
                                          unit);
}

static RGBA32 ParseColorStringWithCrazyLegacyRules(const String& color_string) {
  // Per spec, only look at the first 128 digits of the string.
  const size_t kMaxColorLength = 128;
  // We'll pad the buffer with two extra 0s later, so reserve two more than the
  // max.
  Vector<char, kMaxColorLength + 2> digit_buffer;

  wtf_size_t i = 0;
  // Skip a leading #.
  if (color_string[0] == '#')
    i = 1;

  // Grab the first 128 characters, replacing non-hex characters with 0.
  // Non-BMP characters are replaced with "00" due to them appearing as two
  // "characters" in the String.
  for (; i < color_string.length() && digit_buffer.size() < kMaxColorLength;
       i++) {
    if (!IsASCIIHexDigit(color_string[i]))
      digit_buffer.push_back('0');
    else
      digit_buffer.push_back(color_string[i]);
  }

  if (!digit_buffer.size())
    return Color::kBlack;

  // Pad the buffer out to at least the next multiple of three in size.
  digit_buffer.push_back('0');
  digit_buffer.push_back('0');

  if (digit_buffer.size() < 6)
    return MakeRGB(ToASCIIHexValue(digit_buffer[0]),
                   ToASCIIHexValue(digit_buffer[1]),
                   ToASCIIHexValue(digit_buffer[2]));

  // Split the digits into three components, then search the last 8 digits of
  // each component.
  DCHECK_GE(digit_buffer.size(), 6u);
  wtf_size_t component_length = digit_buffer.size() / 3;
  wtf_size_t component_search_window_length =
      std::min<wtf_size_t>(component_length, 8);
  wtf_size_t red_index = component_length - component_search_window_length;
  wtf_size_t green_index =
      component_length * 2 - component_search_window_length;
  wtf_size_t blue_index = component_length * 3 - component_search_window_length;
  // Skip digits until one of them is non-zero, or we've only got two digits
  // left in the component.
  while (digit_buffer[red_index] == '0' && digit_buffer[green_index] == '0' &&
         digit_buffer[blue_index] == '0' &&
         (component_length - red_index) > 2) {
    red_index++;
    green_index++;
    blue_index++;
  }
  DCHECK_LT(red_index + 1, component_length);
  DCHECK_GE(green_index, component_length);
  DCHECK_LT(green_index + 1, component_length * 2);
  DCHECK_GE(blue_index, component_length * 2);
  SECURITY_DCHECK(blue_index + 1 < digit_buffer.size());

  int red_value =
      ToASCIIHexValue(digit_buffer[red_index], digit_buffer[red_index + 1]);
  int green_value =
      ToASCIIHexValue(digit_buffer[green_index], digit_buffer[green_index + 1]);
  int blue_value =
      ToASCIIHexValue(digit_buffer[blue_index], digit_buffer[blue_index + 1]);
  return MakeRGB(red_value, green_value, blue_value);
}

// Color parsing that matches HTML's "rules for parsing a legacy color value"
bool HTMLElement::ParseColorWithLegacyRules(const String& attribute_value,
                                            Color& parsed_color) {
  // An empty string doesn't apply a color. (One containing only whitespace
  // does, which is why this check occurs before stripping.)
  if (attribute_value.IsEmpty())
    return false;

  String color_string = attribute_value.StripWhiteSpace();

  // "transparent" doesn't apply a color either.
  if (DeprecatedEqualIgnoringCase(color_string, "transparent"))
    return false;

  // If the string is a 3/6-digit hex color or a named CSS color, use that.
  // Apply legacy rules otherwise. Note color.setFromString() accepts 4/8-digit
  // hex color, so restrict its use with length checks here to support legacy
  // HTML attributes.

  bool success = false;
  if ((color_string.length() == 4 || color_string.length() == 7) &&
      color_string[0] == '#')
    success = parsed_color.SetFromString(color_string);
  if (!success)
    success = parsed_color.SetNamedColor(color_string);
  if (!success) {
    parsed_color.SetRGB(ParseColorStringWithCrazyLegacyRules(color_string));
    success = true;
  }

  return success;
}

void HTMLElement::AddHTMLColorToStyle(MutableCSSPropertyValueSet* style,
                                      CSSPropertyID property_id,
                                      const String& attribute_value) {
  Color parsed_color;
  if (!ParseColorWithLegacyRules(attribute_value, parsed_color))
    return;

  style->SetProperty(property_id, *CSSColorValue::Create(parsed_color.Rgb()));
}

LabelsNodeList* HTMLElement::labels() {
  if (!IsLabelable())
    return nullptr;
  return EnsureCachedCollection<LabelsNodeList>(kLabelsNodeListType);
}

bool HTMLElement::IsInteractiveContent() const {
  return false;
}

void HTMLElement::DefaultEventHandler(Event& event) {
  if (event.type() == event_type_names::kKeypress && event.IsKeyboardEvent()) {
    HandleKeypressEvent(ToKeyboardEvent(event));
    if (event.DefaultHandled())
      return;
  }

  Element::DefaultEventHandler(event);
}

bool HTMLElement::MatchesReadOnlyPseudoClass() const {
  return !MatchesReadWritePseudoClass();
}

bool HTMLElement::MatchesReadWritePseudoClass() const {
  if (FastHasAttribute(kContenteditableAttr)) {
    const AtomicString& value = FastGetAttribute(kContenteditableAttr);

    if (value.IsEmpty() || DeprecatedEqualIgnoringCase(value, "true") ||
        DeprecatedEqualIgnoringCase(value, "plaintext-only"))
      return true;
    if (DeprecatedEqualIgnoringCase(value, "false"))
      return false;
    // All other values should be treated as "inherit".
  }

  return parentElement() && HasEditableStyle(*parentElement());
}

void HTMLElement::HandleKeypressEvent(KeyboardEvent& event) {
  if (!IsSpatialNavigationEnabled(GetDocument().GetFrame()) || !SupportsFocus())
    return;
  if (RuntimeEnabledFeatures::FocuslessSpatialNavigationEnabled())
    return;
  GetDocument().UpdateStyleAndLayoutTree();
  // if the element is a text form control (like <input type=text> or
  // <textarea>) or has contentEditable attribute on, we should enter a space or
  // newline even in spatial navigation mode instead of handling it as a "click"
  // action.
  if (IsTextControl() || HasEditableStyle(*this))
    return;
  int char_code = event.charCode();
  if (char_code == '\r' || char_code == ' ') {
    DispatchSimulatedClick(&event);
    event.SetDefaultHandled();
  }
}

int HTMLElement::offsetLeftForBinding() {
  GetDocument().EnsurePaintLocationDataValidForNode(this);
  Element* offset_parent = unclosedOffsetParent();
  if (LayoutBoxModelObject* layout_object = GetLayoutBoxModelObject())
    return AdjustForAbsoluteZoom::AdjustLayoutUnit(
               LayoutUnit(layout_object->PixelSnappedOffsetLeft(offset_parent)),
               layout_object->StyleRef())
        .Round();
  return 0;
}

int HTMLElement::offsetTopForBinding() {
  GetDocument().EnsurePaintLocationDataValidForNode(this);
  Element* offset_parent = unclosedOffsetParent();
  if (LayoutBoxModelObject* layout_object = GetLayoutBoxModelObject())
    return AdjustForAbsoluteZoom::AdjustLayoutUnit(
               LayoutUnit(layout_object->PixelSnappedOffsetTop(offset_parent)),
               layout_object->StyleRef())
        .Round();
  return 0;
}

int HTMLElement::offsetWidthForBinding() {
  GetDocument().EnsurePaintLocationDataValidForNode(this);
  Element* offset_parent = unclosedOffsetParent();
  if (LayoutBoxModelObject* layout_object = GetLayoutBoxModelObject())
    return AdjustForAbsoluteZoom::AdjustLayoutUnit(
               LayoutUnit(
                   layout_object->PixelSnappedOffsetWidth(offset_parent)),
               layout_object->StyleRef())
        .Round();
  return 0;
}

DISABLE_CFI_PERF
int HTMLElement::offsetHeightForBinding() {
  GetDocument().EnsurePaintLocationDataValidForNode(this);
  Element* offset_parent = unclosedOffsetParent();
  if (LayoutBoxModelObject* layout_object = GetLayoutBoxModelObject())
    return AdjustForAbsoluteZoom::AdjustLayoutUnit(
               LayoutUnit(
                   layout_object->PixelSnappedOffsetHeight(offset_parent)),
               layout_object->StyleRef())
        .Round();
  return 0;
}

Element* HTMLElement::unclosedOffsetParent() {
  GetDocument().UpdateStyleAndLayoutForNode(this);

  LayoutObject* layout_object = GetLayoutObject();
  if (!layout_object)
    return nullptr;

  return layout_object->OffsetParent(this);
}

void HTMLElement::OnDirAttrChanged(const AttributeModificationParams& params) {
  // If an ancestor has dir=auto, and this node has the first character,
  // changes to dir attribute may affect the ancestor.
  if (!CanParticipateInFlatTree())
    return;
  UpdateDistributionForFlatTreeTraversal();
  auto* parent =
      DynamicTo<HTMLElement>(FlatTreeTraversal::ParentElement(*this));
  if (parent && parent->SelfOrAncestorHasDirAutoAttribute()) {
    parent->AdjustDirectionalityIfNeededAfterChildAttributeChanged(this);
  }

  if (DeprecatedEqualIgnoringCase(params.new_value, "auto"))
    CalculateAndAdjustDirectionality();
}

void HTMLElement::OnFormAttrChanged(const AttributeModificationParams& params) {
  if (IsFormAssociatedCustomElement())
    EnsureElementInternals().FormAttributeChanged();
}

void HTMLElement::OnInertAttrChanged(
    const AttributeModificationParams& params) {
  UpdateDistributionForUnknownReasons();
  if (GetDocument().GetFrame()) {
    GetDocument().GetFrame()->SetIsInert(GetDocument().LocalOwner() &&
                                         GetDocument().LocalOwner()->IsInert());
  }
}

void HTMLElement::OnLangAttrChanged(const AttributeModificationParams& params) {
  PseudoStateChanged(CSSSelector::kPseudoLang);
}

void HTMLElement::OnNonceAttrChanged(
    const AttributeModificationParams& params) {
  if (params.new_value != g_empty_atom)
    setNonce(params.new_value);
}

void HTMLElement::OnTabIndexAttrChanged(
    const AttributeModificationParams& params) {
  Element::ParseAttribute(params);
}

void HTMLElement::OnXMLLangAttrChanged(
    const AttributeModificationParams& params) {
  Element::ParseAttribute(params);
}

ElementInternals* HTMLElement::attachInternals(
    ExceptionState& exception_state) {
  if (IsValue()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "Unable to attach ElementInternals to a customized built-in element.");
    return nullptr;
  }
  CustomElementRegistry* registry = CustomElement::Registry(*this);
  auto* definition =
      registry ? registry->DefinitionForName(localName()) : nullptr;
  if (!definition) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "Unable to attach ElementInternals to non-custom elements.");
    return nullptr;
  }
  if (definition->DisableInternals()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "ElementInternals is disabled by disabledFeature static field.");
    return nullptr;
  }
  if (DidAttachInternals()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "ElementInternals for the specified element was already attached.");
    return nullptr;
  }
  SetDidAttachInternals();
  return &EnsureElementInternals();
}

bool HTMLElement::IsFormAssociatedCustomElement() const {
  return GetCustomElementState() == CustomElementState::kCustom &&
         GetCustomElementDefinition()->IsFormAssociated();
}

bool HTMLElement::SupportsFocus() const {
  return Element::SupportsFocus() && !IsDisabledFormControl();
}

bool HTMLElement::IsDisabledFormControl() const {
  if (!IsFormAssociatedCustomElement())
    return false;
  return const_cast<HTMLElement*>(this)
      ->EnsureElementInternals()
      .IsActuallyDisabled();
}

bool HTMLElement::MatchesEnabledPseudoClass() const {
  return IsFormAssociatedCustomElement() && !const_cast<HTMLElement*>(this)
                                                 ->EnsureElementInternals()
                                                 .IsActuallyDisabled();
}

bool HTMLElement::MatchesValidityPseudoClasses() const {
  return IsFormAssociatedCustomElement();
}

bool HTMLElement::willValidate() const {
  return IsFormAssociatedCustomElement() && const_cast<HTMLElement*>(this)
                                                ->EnsureElementInternals()
                                                .WillValidate();
}

bool HTMLElement::IsValidElement() {
  return IsFormAssociatedCustomElement() &&
         EnsureElementInternals().IsValidElement();
}

bool HTMLElement::IsLabelable() const {
  return IsFormAssociatedCustomElement();
}

void HTMLElement::FinishParsingChildren() {
  Element::FinishParsingChildren();
  if (IsFormAssociatedCustomElement())
    EnsureElementInternals().TakeStateAndRestore();
}

}  // namespace blink

#ifndef NDEBUG

// For use in the debugger
void dumpInnerHTML(blink::HTMLElement*);

void dumpInnerHTML(blink::HTMLElement* element) {
  printf("%s\n", element->InnerHTMLAsString().Ascii().c_str());
}
#endif
