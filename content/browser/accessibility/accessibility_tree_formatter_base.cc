// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_formatter_base.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/accessibility/accessibility_tree_formatter_blink.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents.h"

namespace content {

namespace {

const char kIndentSymbol = '+';
const int kIndentSymbolCount = 2;
const char kSkipString[] = "@NO_DUMP";
const char kSkipChildren[] = "@NO_CHILDREN_DUMP";

}  // namespace

//
// PropertyNode
//

// static
PropertyNode PropertyNode::FromProperty(const base::string16& property) {
  PropertyNode root;
  Parse(&root, property.begin(), property.end());

  PropertyNode* node = &root.parameters[0];
  node->original_property = property;
  return std::move(*node);
}

PropertyNode::PropertyNode() = default;
PropertyNode::PropertyNode(PropertyNode&& o)
    : value(std::move(o.value)),
      parameters(std::move(o.parameters)),
      original_property(std::move(o.original_property)) {}
PropertyNode::~PropertyNode() = default;

PropertyNode& PropertyNode::operator=(PropertyNode&& o) {
  value = std::move(o.value);
  parameters = std::move(o.parameters);
  original_property = std::move(o.original_property);
  return *this;
}

PropertyNode::operator bool() const {
  return !value.empty();
}

std::string PropertyNode::ToString() const {
  std::string out = base::UTF16ToUTF8(value);
  if (parameters.size()) {
    out += '(';
    for (size_t i = 0; i < parameters.size(); i++) {
      if (i != 0) {
        out += ", ";
      }
      out += parameters[i].ToString();
    }
    out += ')';
  }
  return out;
}

// private
PropertyNode::PropertyNode(const base::string16& v) : value(v) {}
PropertyNode::PropertyNode(PropertyNode::iterator begin,
                           PropertyNode::iterator end)
    : value(begin, end) {}

// private static
PropertyNode::iterator PropertyNode::Parse(PropertyNode* node,
                                           PropertyNode::iterator begin,
                                           PropertyNode::iterator end) {
  auto iter = begin;
  while (iter != end) {
    // Subnode begins: create a new node, record its name and parse its
    // arguments.
    if (*iter == '(') {
      node->parameters.push_back(PropertyNode(begin, iter));
      begin = iter = Parse(&node->parameters.back(), ++iter, end);
      continue;
    }

    // Subnode begins: a special case for arrays, which have [arg1, ..., argN]
    // form.
    if (*iter == '[') {
      node->parameters.push_back(PropertyNode(base::UTF8ToUTF16("[]")));
      begin = iter = Parse(&node->parameters.back(), ++iter, end);
      continue;
    }

    // Subnode ends.
    if (*iter == ')' || *iter == ']') {
      if (begin != iter) {
        node->parameters.push_back(PropertyNode(begin, iter));
      }
      return ++iter;
    }

    // Skip spaces, adjust new node start.
    if (*iter == ' ') {
      begin = ++iter;
    }

    // Subsequent scalar param case.
    if (*iter == ',' && begin != iter) {
      node->parameters.push_back(PropertyNode(begin, iter));
      iter++;
      begin = iter;
      continue;
    }

    iter++;
  }

  // Single scalar param case.
  if (begin != iter) {
    node->parameters.push_back(PropertyNode(begin, iter));
  }
  return iter;
}

// AccessibilityTreeFormatter

AccessibilityTreeFormatter::TestPass AccessibilityTreeFormatter::GetTestPass(
    size_t index) {
  std::vector<content::AccessibilityTreeFormatter::TestPass> passes =
      content::AccessibilityTreeFormatter::GetTestPasses();
  CHECK_LT(index, passes.size());
  return passes[index];
}

// static
base::string16 AccessibilityTreeFormatterBase::DumpAccessibilityTreeFromManager(
    BrowserAccessibilityManager* ax_mgr,
    bool internal,
    std::vector<PropertyFilter> property_filters) {
  std::unique_ptr<AccessibilityTreeFormatter> formatter;
  if (internal)
    formatter = std::make_unique<AccessibilityTreeFormatterBlink>();
  else
    formatter = Create();
  base::string16 accessibility_contents_utf16;
  formatter->SetPropertyFilters(property_filters);
  std::unique_ptr<base::DictionaryValue> dict =
      static_cast<AccessibilityTreeFormatterBase*>(formatter.get())
          ->BuildAccessibilityTree(ax_mgr->GetRoot());
  formatter->FormatAccessibilityTree(*dict, &accessibility_contents_utf16);
  return accessibility_contents_utf16;
}

bool AccessibilityTreeFormatter::MatchesPropertyFilters(
    const std::vector<PropertyFilter>& property_filters,
    const base::string16& text,
    bool default_result) {
  bool allow = default_result;
  for (const auto& filter : property_filters) {
    if (base::MatchPattern(text, filter.match_str)) {
      switch (filter.type) {
        case PropertyFilter::ALLOW_EMPTY:
          allow = true;
          break;
        case PropertyFilter::ALLOW:
          allow = (!base::MatchPattern(text, base::UTF8ToUTF16("*=''")));
          break;
        case PropertyFilter::DENY:
          allow = false;
          break;
      }
    }
  }
  return allow;
}

bool AccessibilityTreeFormatter::MatchesNodeFilters(
    const std::vector<NodeFilter>& node_filters,
    const base::DictionaryValue& dict) {
  for (const auto& filter : node_filters) {
    base::string16 value;
    if (!dict.GetString(filter.property, &value)) {
      continue;
    }
    if (base::MatchPattern(value, filter.pattern)) {
      return true;
    }
  }
  return false;
}

AccessibilityTreeFormatterBase::AccessibilityTreeFormatterBase() = default;

AccessibilityTreeFormatterBase::~AccessibilityTreeFormatterBase() = default;

void AccessibilityTreeFormatterBase::FormatAccessibilityTree(
    const base::DictionaryValue& dict,
    base::string16* contents) {
  RecursiveFormatAccessibilityTree(dict, contents);
}

void AccessibilityTreeFormatterBase::FormatAccessibilityTreeForTesting(
    ui::AXPlatformNodeDelegate* root,
    base::string16* contents) {
  auto* node_internal = BrowserAccessibility::FromAXPlatformNodeDelegate(root);
  DCHECK(node_internal);
  FormatAccessibilityTree(*BuildAccessibilityTree(node_internal), contents);
}

std::unique_ptr<base::DictionaryValue>
AccessibilityTreeFormatterBase::FilterAccessibilityTree(
    const base::DictionaryValue& dict) {
  auto filtered_dict = std::make_unique<base::DictionaryValue>();
  ProcessTreeForOutput(dict, filtered_dict.get());
  const base::ListValue* children;
  if (dict.GetList(kChildrenDictAttr, &children) && !children->empty()) {
    const base::DictionaryValue* child_dict;
    auto filtered_children = std::make_unique<base::ListValue>();
    for (size_t i = 0; i < children->GetSize(); i++) {
      children->GetDictionary(i, &child_dict);
      auto filtered_child = FilterAccessibilityTree(*child_dict);
      filtered_children->Append(std::move(filtered_child));
    }
    filtered_dict->Set(kChildrenDictAttr, std::move(filtered_children));
  }
  return filtered_dict;
}

void AccessibilityTreeFormatterBase::RecursiveFormatAccessibilityTree(
    const base::DictionaryValue& dict,
    base::string16* contents,
    int depth) {
  // Check dictionary against node filters, may require us to skip this node
  // and its children.
  if (MatchesNodeFilters(dict))
    return;

  base::string16 indent =
      base::string16(depth * kIndentSymbolCount, kIndentSymbol);
  base::string16 line = indent + ProcessTreeForOutput(dict);
  if (line.find(base::ASCIIToUTF16(kSkipString)) != base::string16::npos)
    return;

  // Normalize any Windows-style line endings by removing \r.
  base::RemoveChars(line, base::ASCIIToUTF16("\r"), &line);

  // Replace literal newlines with "<newline>"
  base::ReplaceChars(line, base::ASCIIToUTF16("\n"),
                     base::ASCIIToUTF16("<newline>"), &line);

  *contents += line + base::ASCIIToUTF16("\n");
  if (line.find(base::ASCIIToUTF16(kSkipChildren)) != base::string16::npos)
    return;

  const base::ListValue* children;
  if (!dict.GetList(kChildrenDictAttr, &children))
    return;
  const base::DictionaryValue* child_dict;
  for (size_t i = 0; i < children->GetSize(); i++) {
    children->GetDictionary(i, &child_dict);
    RecursiveFormatAccessibilityTree(*child_dict, contents, depth + 1);
  }
}

void AccessibilityTreeFormatterBase::SetPropertyFilters(
    const std::vector<PropertyFilter>& property_filters) {
  property_filters_ = property_filters;
}

void AccessibilityTreeFormatterBase::SetNodeFilters(
    const std::vector<NodeFilter>& node_filters) {
  node_filters_ = node_filters;
}

void AccessibilityTreeFormatterBase::set_show_ids(bool show_ids) {
  show_ids_ = show_ids;
}

base::FilePath::StringType
AccessibilityTreeFormatterBase::GetVersionSpecificExpectedFileSuffix() {
  return FILE_PATH_LITERAL("");
}

PropertyNode AccessibilityTreeFormatterBase::GetMatchingPropertyNode(
    const base::string16& text) {
  // Find the first allow-filter matching the property name.
  // The filters have form of name(args)=value. Here we match the name part.
  const base::string16 filter_delim = base::ASCIIToUTF16("=");
  for (const auto& filter : property_filters_) {
    base::String16Tokenizer filter_tokenizer(filter.match_str, filter_delim);
    if (!filter_tokenizer.GetNext()) {
      continue;
    }

    base::string16 property = filter_tokenizer.token();
    PropertyNode property_node = PropertyNode::FromProperty(property);

    // The filter should be either an exact property match or a wildcard
    // matching to support filter collections like AXRole* which matches
    // AXRoleDescription.
    if (text == property_node.value ||
        base::MatchPattern(text, property_node.value)) {
      switch (filter.type) {
        case PropertyFilter::ALLOW_EMPTY:
        case PropertyFilter::ALLOW:
          return property_node;
        case PropertyFilter::DENY:
          break;
        default:
          break;
      }
    }
  }
  return PropertyNode();
}

bool AccessibilityTreeFormatterBase::MatchesPropertyFilters(
    const base::string16& text,
    bool default_result) const {
  return AccessibilityTreeFormatter::MatchesPropertyFilters(
      property_filters_, text, default_result);
}

bool AccessibilityTreeFormatterBase::MatchesNodeFilters(
    const base::DictionaryValue& dict) const {
  return AccessibilityTreeFormatter::MatchesNodeFilters(node_filters_, dict);
}

base::string16 AccessibilityTreeFormatterBase::FormatCoordinates(
    const base::DictionaryValue& value,
    const std::string& name,
    const std::string& x_name,
    const std::string& y_name) {
  int x, y;
  value.GetInteger(x_name, &x);
  value.GetInteger(y_name, &y);
  std::string xy_str(base::StringPrintf("%s=(%d, %d)", name.c_str(), x, y));

  return base::UTF8ToUTF16(xy_str);
}

base::string16 AccessibilityTreeFormatterBase::FormatRectangle(
    const base::DictionaryValue& value,
    const std::string& name,
    const std::string& left_name,
    const std::string& top_name,
    const std::string& width_name,
    const std::string& height_name) {
  int left, top, width, height;
  value.GetInteger(left_name, &left);
  value.GetInteger(top_name, &top);
  value.GetInteger(width_name, &width);
  value.GetInteger(height_name, &height);
  std::string rect_str(base::StringPrintf("%s=(%d, %d, %d, %d)", name.c_str(),
                                          left, top, width, height));

  return base::UTF8ToUTF16(rect_str);
}

bool AccessibilityTreeFormatterBase::WriteAttribute(bool include_by_default,
                                                    const std::string& attr,
                                                    base::string16* line) {
  return WriteAttribute(include_by_default, base::UTF8ToUTF16(attr), line);
}

bool AccessibilityTreeFormatterBase::WriteAttribute(bool include_by_default,
                                                    const base::string16& attr,
                                                    base::string16* line) {
  if (attr.empty())
    return false;
  if (!MatchesPropertyFilters(attr, include_by_default))
    return false;
  if (!line->empty())
    *line += base::ASCIIToUTF16(" ");
  *line += attr;
  return true;
}

void AccessibilityTreeFormatterBase::AddPropertyFilter(
    std::vector<PropertyFilter>* property_filters,
    std::string filter,
    PropertyFilter::Type type) {
  property_filters->push_back(PropertyFilter(base::ASCIIToUTF16(filter), type));
}

void AccessibilityTreeFormatterBase::AddDefaultFilters(
    std::vector<PropertyFilter>* property_filters) {}

}  // namespace content
