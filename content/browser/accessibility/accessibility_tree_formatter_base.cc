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
PropertyNode PropertyNode::FromPropertyFilter(
    const AccessibilityTreeFormatter::PropertyFilter& filter) {
  // Property invocation: property_str expected format is
  // prop_name or prop_name(arg1, ... argN).
  PropertyNode root;
  Parse(&root, filter.property_str.begin(), filter.property_str.end());

  PropertyNode* node = &root.parameters[0];
  node->original_property = filter.property_str;

  // Line indexes filter: filter_str expected format is
  // :line_num_1, ... :line_num_N, a comma separated list of line indexes
  // the property should be queried for. For example, ":1,:5,:7" indicates that
  // the property should called for objects placed on 1, 5 and 7 lines only.
  if (!filter.filter_str.empty()) {
    node->line_indexes =
        base::SplitString(filter.filter_str, base::string16(1, ','),
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  }

  return std::move(*node);
}

PropertyNode::PropertyNode() = default;
PropertyNode::PropertyNode(PropertyNode&& o)
    : key(std::move(o.key)),
      name_or_value(std::move(o.name_or_value)),
      parameters(std::move(o.parameters)),
      original_property(std::move(o.original_property)),
      line_indexes(std::move(o.line_indexes)) {}
PropertyNode::~PropertyNode() = default;

PropertyNode& PropertyNode::operator=(PropertyNode&& o) {
  key = std::move(o.key);
  name_or_value = std::move(o.name_or_value);
  parameters = std::move(o.parameters);
  original_property = std::move(o.original_property);
  line_indexes = std::move(o.line_indexes);
  return *this;
}

PropertyNode::operator bool() const {
  return !name_or_value.empty();
}

bool PropertyNode::IsArray() const {
  return name_or_value == base::ASCIIToUTF16("[]");
}

bool PropertyNode::IsDict() const {
  return name_or_value == base::ASCIIToUTF16("{}");
}

base::Optional<int> PropertyNode::AsInt() const {
  int value = 0;
  if (!base::StringToInt(name_or_value, &value)) {
    return base::nullopt;
  }
  return value;
}

base::Optional<base::string16> PropertyNode::FindKey(const char* refkey) const {
  for (const auto& param : parameters) {
    if (param.key == base::ASCIIToUTF16(refkey)) {
      return param.name_or_value;
    }
  }
  return base::nullopt;
}

base::Optional<int> PropertyNode::FindIntKey(const char* refkey) const {
  for (const auto& param : parameters) {
    if (param.key == base::ASCIIToUTF16(refkey)) {
      return param.AsInt();
    }
  }
  return base::nullopt;
}

std::string PropertyNode::ToString() const {
  std::string out;
  for (const auto& index : line_indexes) {
    if (!out.empty()) {
      out += ',';
    }
    out += base::UTF16ToUTF8(index);
  }
  if (!out.empty()) {
    out += ';';
  }

  if (!key.empty()) {
    out += base::UTF16ToUTF8(key) + ": ";
  }
  out += base::UTF16ToUTF8(name_or_value);
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
PropertyNode::PropertyNode(PropertyNode::iterator key_begin,
                           PropertyNode::iterator key_end,
                           const base::string16& name_or_value)
    : key(key_begin, key_end), name_or_value(name_or_value) {}
PropertyNode::PropertyNode(PropertyNode::iterator begin,
                           PropertyNode::iterator end)
    : name_or_value(begin, end) {}
PropertyNode::PropertyNode(PropertyNode::iterator key_begin,
                           PropertyNode::iterator key_end,
                           PropertyNode::iterator value_begin,
                           PropertyNode::iterator value_end)
    : key(key_begin, key_end), name_or_value(value_begin, value_end) {}

// private static
PropertyNode::iterator PropertyNode::Parse(PropertyNode* node,
                                           PropertyNode::iterator begin,
                                           PropertyNode::iterator end) {
  auto iter = begin;
  auto key_begin = end, key_end = end;
  while (iter != end) {
    // Subnode begins: create a new node, record its name and parse its
    // arguments.
    if (*iter == '(') {
      node->parameters.push_back(PropertyNode(key_begin, key_end, begin, iter));
      key_begin = key_end = end;
      begin = iter = Parse(&node->parameters.back(), ++iter, end);
      continue;
    }

    // Subnode begins: a special case for arrays, which have [arg1, ..., argN]
    // form.
    if (*iter == '[') {
      node->parameters.push_back(
          PropertyNode(key_begin, key_end, base::UTF8ToUTF16("[]")));
      key_begin = key_end = end;
      begin = iter = Parse(&node->parameters.back(), ++iter, end);
      continue;
    }

    // Subnode begins: a special case for dictionaries of {key1: value1, ...,
    // key2: value2} form.
    if (*iter == '{') {
      node->parameters.push_back(
          PropertyNode(key_begin, key_end, base::UTF8ToUTF16("{}")));
      key_begin = key_end = end;
      begin = iter = Parse(&node->parameters.back(), ++iter, end);
      continue;
    }

    // Subnode ends.
    if (*iter == ')' || *iter == ']' || *iter == '}') {
      if (begin != iter) {
        node->parameters.push_back(
            PropertyNode(key_begin, key_end, begin, iter));
        key_begin = key_end = end;
      }
      return ++iter;
    }

    // Dictionary key
    auto maybe_key_end = end;
    if (*iter == ':') {
      maybe_key_end = iter++;
    }

    // Skip spaces, adjust new node start.
    if (*iter == ' ') {
      if (maybe_key_end != end) {
        key_begin = begin;
        key_end = maybe_key_end;
      }
      begin = ++iter;
      continue;
    }

    // Subsequent scalar param case.
    if (*iter == ',' && begin != iter) {
      node->parameters.push_back(PropertyNode(key_begin, key_end, begin, iter));
      iter++;
      key_begin = key_end = end;
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

//
// AccessibilityTreeFormatter
//

AccessibilityTreeFormatter::PropertyFilter::PropertyFilter(
    const PropertyFilter&) = default;

AccessibilityTreeFormatter::PropertyFilter::PropertyFilter(
    const base::string16& str,
    Type type)
    : match_str(str), type(type) {
  size_t index = str.find(';');
  if (index != std::string::npos) {
    filter_str = str.substr(0, index);
    if (index + 1 < str.length()) {
      match_str = str.substr(index + 1, std::string::npos);
    }
  }
  property_str = match_str.substr(0, match_str.find('='));
}

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
    const base::string16& line_index,
    const base::string16& property_name) {
  // Find the first allow-filter matching the line index and the property name.
  for (const auto& filter : property_filters_) {
    PropertyNode property_node = PropertyNode::FromPropertyFilter(filter);

    // Skip if the line index filter doesn't matched (if specified).
    if (!property_node.line_indexes.empty() &&
        std::find(property_node.line_indexes.begin(),
                  property_node.line_indexes.end(),
                  line_index) == property_node.line_indexes.end()) {
      continue;
    }

    // The filter should be either an exact property match or a wildcard
    // matching to support filter collections like AXRole* which matches
    // AXRoleDescription.
    if (property_name == property_node.name_or_value ||
        base::MatchPattern(property_name, property_node.name_or_value)) {
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
