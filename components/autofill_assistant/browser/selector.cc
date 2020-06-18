// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/selector.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"

namespace autofill_assistant {

namespace {

bool operator<(const SelectorProto::TextFilter& a,
               const SelectorProto::TextFilter& b) {
  return std::make_tuple(a.re2(), a.case_sensitive()) <
         std::make_tuple(b.re2(), b.case_sensitive());
}

bool operator<(const SelectorProto::Filter& a, const SelectorProto::Filter& b) {
  if (a.filter_case() < b.filter_case()) {
    return true;
  }
  if (a.filter_case() != b.filter_case()) {
    return false;
  }
  switch (a.filter_case()) {
    case SelectorProto::Filter::kCssSelector:
      return a.css_selector() < b.css_selector();

    case SelectorProto::Filter::kInnerText:
      return a.inner_text() < b.inner_text();

    case SelectorProto::Filter::kValue:
      return a.value() < b.value();

    case SelectorProto::Filter::kPseudoType:
      return a.pseudo_type() < b.pseudo_type();

    case SelectorProto::Filter::kPseudoElementContent:
      if (a.pseudo_element_content().pseudo_type() <
          b.pseudo_element_content().pseudo_type()) {
        return true;
      }
      if (a.pseudo_element_content().pseudo_type() !=
          b.pseudo_element_content().pseudo_type()) {
        return false;
      }
      return a.pseudo_element_content().content() <
             b.pseudo_element_content().content();

    case SelectorProto::Filter::kBoundingBox:
    case SelectorProto::Filter::kEnterFrame:
    case SelectorProto::Filter::kPickOne:
    case SelectorProto::Filter::kLabelled:
      return false;

    case SelectorProto::Filter::FILTER_NOT_SET:
      return false;
  }
  return false;
}

#ifndef NDEBUG

std::ostream& operator<<(std::ostream& out, PseudoType pseudo_type) {
  return out << PseudoTypeName(pseudo_type);
}

std::ostream& operator<<(std::ostream& out,
                         const SelectorProto::TextFilter& c) {
  out << "/" << c.re2() << "/";
  if (c.case_sensitive()) {
    out << "i";
  }
  return out;
}

std::ostream& operator<<(std::ostream& out, const SelectorProto::Filter& f) {
  switch (f.filter_case()) {
    case SelectorProto::Filter::kEnterFrame:
      out << "/";
      return out;

    case SelectorProto::Filter::kCssSelector:
      out << f.css_selector();
      return out;

    case SelectorProto::Filter::kInnerText:
      out << "innerText~=" << f.inner_text();
      return out;

    case SelectorProto::Filter::kValue:
      out << "value~=" << f.value();
      return out;

    case SelectorProto::Filter::kPseudoType:
      out << "::" << f.pseudo_type();
      return out;

    case SelectorProto::Filter::kPseudoElementContent:
      out << "::" << f.pseudo_element_content().pseudo_type()
          << "~=" << f.pseudo_element_content().content();
      return out;

    case SelectorProto::Filter::kBoundingBox:
      out << "bounding_box";
      return out;

    case SelectorProto::Filter::kPickOne:
      out << "pick_one";
      return out;

    case SelectorProto::Filter::kLabelled:
      out << "labelled";
      return out;

    case SelectorProto::Filter::FILTER_NOT_SET:
      // Either unset or set to an unsupported value. Let's assume the worse.
      out << "INVALID";
      return out;
  }
}
#endif  // NDEBUG
}  // namespace

SelectorProto ToSelectorProto(const std::string& s) {
  return ToSelectorProto(std::vector<std::string>{s});
}

SelectorProto ToSelectorProto(const std::vector<std::string>& s) {
  SelectorProto proto;
  if (!s.empty()) {
    for (size_t i = 0; i < s.size(); i++) {
      if (i > 0) {
        proto.add_filters()->mutable_pick_one();
        proto.add_filters()->mutable_enter_frame();
      }
      proto.add_filters()->set_css_selector(s[i]);
    }
  }
  return proto;
}

std::string PseudoTypeName(PseudoType pseudo_type) {
  switch (pseudo_type) {
    case UNDEFINED:
      return "undefined";
    case FIRST_LINE:
      return "first-line";
    case FIRST_LETTER:
      return "first-letter";
    case BEFORE:
      return "before";
    case AFTER:
      return "after";
    case BACKDROP:
      return "backdrop";
    case SELECTION:
      return "selection";
    case FIRST_LINE_INHERITED:
      return "first-line-inherited";
    case SCROLLBAR:
      return "scrollbar";
    case SCROLLBAR_THUMB:
      return "scrollbar-thumb";
    case SCROLLBAR_BUTTON:
      return "scrollbar-button";
    case SCROLLBAR_TRACK:
      return "scrollbar-track";
    case SCROLLBAR_TRACK_PIECE:
      return "scrollbar-track-piece";
    case SCROLLBAR_CORNER:
      return "scrollbar-corner";
    case RESIZER:
      return "resizer";
    case INPUT_LIST_BUTTON:
      return "input-list-button";

      // Intentionally no default case to make compilation fail if a new value
      // was added to the enum but not to this list.
  }
}

Selector::Selector() {}

Selector::Selector(const SelectorProto& selector_proto)
    : proto(selector_proto) {}

Selector::~Selector() = default;

Selector::Selector(Selector&& other) = default;
Selector::Selector(const Selector& other) = default;
Selector& Selector::operator=(const Selector& other) = default;
Selector& Selector::operator=(Selector&& other) = default;

bool Selector::operator<(const Selector& other) const {
  const auto& a = proto.filters();
  const auto& b = other.proto.filters();
  if (a.size() < b.size()) {
    return true;
  }
  if (a.size() != b.size()) {
    return false;
  }
  for (int i = 0; i < a.size(); i++) {
    const SelectorProto::Filter& filter_a = a.Get(i);
    const SelectorProto::Filter& filter_b = b.Get(i);
    if (filter_a < filter_b) {
      return true;
    }
    if (filter_b < filter_a) {
      return false;
    }
  }
  return false;
}

bool Selector::operator==(const Selector& other) const {
  return !(*this < other) && !(other < *this);
}

Selector& Selector::MustBeVisible() {
  int filter_count = proto.filters().size();
  if (filter_count == 0 || proto.filters(filter_count - 1).has_bounding_box()) {
    // Avoids adding duplicate visibility requirements in the common case.
    return *this;
  }
  proto.add_filters()->mutable_bounding_box();
  return *this;
}

bool Selector::empty() const {
  bool has_css_selector = false;
  for (const SelectorProto::Filter& filter : proto.filters()) {
    switch (filter.filter_case()) {
      case SelectorProto::Filter::FILTER_NOT_SET:
        // There must not be any unknown or invalid filters.
        return true;

      case SelectorProto::Filter::kCssSelector:
        // There must be at least one CSS selector, since it's the only
        // way we have of expanding the set of matches.
        has_css_selector = true;
        break;

      default:
        break;
    }
  }
  return !has_css_selector;
}

base::Optional<std::string> Selector::ExtractSingleCssSelectorForAutofill()
    const {
  int last_enter_frame_index = -1;
  for (int i = proto.filters().size() - 1; i >= 0; i--) {
    if (proto.filters(i).filter_case() == SelectorProto::Filter::kEnterFrame) {
      last_enter_frame_index = i;
      break;
    }
  }
  std::string css_selector;
  for (int i = last_enter_frame_index + 1; i < proto.filters().size(); i++) {
    const SelectorProto::Filter& filter = proto.filters(i);
    switch (filter.filter_case()) {
      case SelectorProto::Filter::kCssSelector:
        if (css_selector.empty()) {
          css_selector = filter.css_selector();
        } else {
          VLOG(1) << __func__
                  << " Selector with multiple CSS selectors not supported for "
                     "autofill: "
                  << *this;
          return base::nullopt;
        }
        break;

      case SelectorProto::Filter::kBoundingBox:
      case SelectorProto::Filter::kPickOne:
        // Ignore these; they're not relevant for the autofill use-case
        break;

      case SelectorProto::Filter::kInnerText:
      case SelectorProto::Filter::kValue:
      case SelectorProto::Filter::kPseudoType:
      case SelectorProto::Filter::kPseudoElementContent:
      case SelectorProto::Filter::kLabelled:
        VLOG(1) << __func__
                << " Selector feature not supported by autofill: " << *this;
        return base::nullopt;

      case SelectorProto::Filter::FILTER_NOT_SET:
        VLOG(1) << __func__ << " Unknown filter type in: " << *this;
        return base::nullopt;

      case SelectorProto::Filter::kEnterFrame:
        // This cannot possibly happen, since the iteration started after the
        // last enter_frame.
        NOTREACHED();
        break;
    }
  }
  if (css_selector.empty()) {
    VLOG(1) << __func__
            << " Selector without CSS selector not supported by autofill: "
            << *this;
    return base::nullopt;
  }
  return css_selector;
}

std::ostream& operator<<(std::ostream& out, const Selector& selector) {
#ifdef NDEBUG
  out << selector.proto.filters().size() << " filter(s).";
  return out;
#else
  out << "[";
  bool first = true;
  for (const SelectorProto::Filter& filter : selector.proto.filters()) {
    if (first) {
      first = false;
    } else {
      out << " ";
    }
    out << filter;
  }
  out << "]";
  return out;
#endif  // NDEBUG
}

}  // namespace autofill_assistant
