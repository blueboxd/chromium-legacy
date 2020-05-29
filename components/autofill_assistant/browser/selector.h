// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SELECTOR_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SELECTOR_H_

#include <ostream>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {

// Convenience functions for creating SelectorProtos.
SelectorProto ToSelectorProto(const std::string& s);

// Convenience wrapper around a SelectorProto that makes it simpler to work with
// selectors.
//
// Selectors are comparables, can be used as std::map key or std::set elements
// and converted to string with operator<<.
struct Selector {
  // A sequence of CSS selectors. Any non-final CSS selector is expected to
  // arrive at a frame or an iframe, i.e. an element that contains another
  // document.
  std::vector<std::string> selectors;

  // If true, only match visible elements. Visible elements are elements that
  // have a box model. The box model is not checked at all, so an element with a
  // zero size bounding box is considered visible.
  bool must_be_visible = false;

  // If non-empty, this must be a regular expression that matches the inner text
  // of the element(s) matching selectors.
  std::string inner_text_pattern;

  // If trie, the |inner_text_pattern| will be checked case sensitively.
  bool inner_text_pattern_case_sensitive = false;

  // If non-empty, this must be a regular expression that matches the value
  // of the element(s) matching selectors.
  std::string value_pattern;

  // If true, the |value_pattern| will be checked case sensitively.
  bool value_pattern_case_sensitive = false;

  // An optional pseudo type. This pseudo type is associated to the final
  // element matched by |selectors|, which means that we currently don't handle
  // matching an element inside a pseudo element.
  PseudoType pseudo_type = PseudoType::UNDEFINED;

  Selector();
  ~Selector();

  explicit Selector(const SelectorProto& proto);
  explicit Selector(const std::vector<std::string>& s);
  Selector(const std::vector<std::string>& s, PseudoType p);

  SelectorProto ToProto() const;

  Selector(Selector&& other);
  Selector(const Selector& other);
  Selector& operator=(Selector&& other);
  Selector& operator=(const Selector& other);

  bool operator<(const Selector& other) const;
  bool operator==(const Selector& other) const;

  // Convenience function to update the visible field in a fluent style.
  Selector& MustBeVisible() {
    must_be_visible = true;
    return *this;
  }

  // The output operator. The actual selectors are only available in debug
  // builds.
  friend std::ostream& operator<<(std::ostream& out, const Selector& selector);

  // Checks whether this selector is empty.
  bool empty() const;

  // Convenience function to set inner_text_pattern in a fluent style.
  Selector& MatchingInnerText(const std::string& pattern) {
    return MatchingInnerText(pattern, false);
  }

  // Convenience function  to set inner_text_pattern matching with case
  // sensitivity.
  Selector& MatchingInnerText(const std::string& pattern, bool case_sensitive) {
    inner_text_pattern = pattern;
    inner_text_pattern_case_sensitive = case_sensitive;
    return *this;
  }

  // Convenience function to set inner_text_pattern in a fluent style.
  Selector& MatchingValue(const std::string& pattern) {
    value_pattern = pattern;
    return *this;
  }

  // Convenience function to set value_pattern matchinng with case sensitivity.
  Selector& MatchingValue(const std::string& pattern, bool case_sensitive) {
    value_pattern = pattern;
    value_pattern_case_sensitive = case_sensitive;
    return *this;
  }

  // Returns a single CSS selector pointing to the element from the last frame,
  // to pass to autofill.
  //
  // This call returns nothing if the selector contains unsupported filters,
  // such as innerText or pseudo-element filters.
  //
  // AutofillAgent::GetElementFormAndFieldData takes a single CSS selector that
  // identifies the form. This means that form elements for autofill are limited
  // to one single CSS selector and no further filtering. TODO(b/155264465):
  // have ElementFinder specify the element it has found in a format that
  // Autofill can recognise.
  base::Optional<std::string> ExtractSingleCssSelectorForAutofill() const;
};
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SELECTOR_H_
