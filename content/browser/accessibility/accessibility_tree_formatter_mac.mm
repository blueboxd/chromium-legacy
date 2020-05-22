// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_formatter_base.h"

#import <Cocoa/Cocoa.h>

#include <re2/re2.h>
#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/browser/accessibility/accessibility_tree_formatter_blink.h"
#include "content/browser/accessibility/browser_accessibility_cocoa.h"
#include "content/browser/accessibility/browser_accessibility_mac.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"

// This file uses the deprecated NSObject accessibility interface.
// TODO(crbug.com/948844): Migrate to the new NSAccessibility interface.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

using base::StringPrintf;
using base::SysNSStringToUTF8;
using base::SysNSStringToUTF16;
using std::string;

namespace content {

namespace {

const char kPositionDictAttr[] = "position";
const char kXCoordDictAttr[] = "x";
const char kYCoordDictAttr[] = "y";
const char kSizeDictAttr[] = "size";
const char kWidthDictAttr[] = "width";
const char kHeightDictAttr[] = "height";
const char kRangeLocDictAttr[] = "loc";
const char kRangeLenDictAttr[] = "len";

}  // namespace

class AccessibilityTreeFormatterMac : public AccessibilityTreeFormatterBase {
 public:
  explicit AccessibilityTreeFormatterMac();
  ~AccessibilityTreeFormatterMac() override;

  void AddDefaultFilters(
      std::vector<PropertyFilter>* property_filters) override;

  std::unique_ptr<base::DictionaryValue> BuildAccessibilityTree(
      BrowserAccessibility* root) override;

  std::unique_ptr<base::DictionaryValue> BuildAccessibilityTreeForProcess(
      base::ProcessId pid) override;
  std::unique_ptr<base::DictionaryValue> BuildAccessibilityTreeForWindow(
      gfx::AcceleratedWidget widget) override;
  std::unique_ptr<base::DictionaryValue> BuildAccessibilityTreeForPattern(
      const base::StringPiece& pattern) override;

 private:
  using LineIndexesMap = std::map<const gfx::NativeViewAccessible, int>;

  void RecursiveBuildAccessibilityTree(const BrowserAccessibilityCocoa* node,
                                       const LineIndexesMap& line_indexes_map,
                                       base::DictionaryValue* dict);
  void RecursiveBuildLineIndexesMap(const BrowserAccessibilityCocoa* node,
                                    LineIndexesMap* line_indexes_map,
                                    int* counter);

  base::FilePath::StringType GetExpectedFileSuffix() override;
  const std::string GetAllowEmptyString() override;
  const std::string GetAllowString() override;
  const std::string GetDenyString() override;
  const std::string GetDenyNodeString() override;

  void AddProperties(const BrowserAccessibilityCocoa* node,
                     const LineIndexesMap& line_indexes_map,
                     base::Value* dict);
  base::Value PopulateSize(const BrowserAccessibilityCocoa*) const;
  base::Value PopulatePosition(const BrowserAccessibilityCocoa*) const;
  base::Value PopulateRange(NSRange) const;
  base::Value PopulateObject(id, const LineIndexesMap& line_indexes_map) const;
  base::Value PopulateArray(NSArray*,
                            const LineIndexesMap& line_indexes_map) const;

  base::string16 ProcessTreeForOutput(
      const base::DictionaryValue& node,
      base::DictionaryValue* filtered_dict_result = nullptr) override;

  std::string FormatAttributeValue(const base::Value& value);
};

// static
std::unique_ptr<AccessibilityTreeFormatter>
AccessibilityTreeFormatter::Create() {
  return std::make_unique<AccessibilityTreeFormatterMac>();
}

// static
std::vector<AccessibilityTreeFormatter::TestPass>
AccessibilityTreeFormatter::GetTestPasses() {
  return {
      {"blink", &AccessibilityTreeFormatterBlink::CreateBlink},
      {"mac", &AccessibilityTreeFormatter::Create},
  };
}

AccessibilityTreeFormatterMac::AccessibilityTreeFormatterMac() {}

AccessibilityTreeFormatterMac::~AccessibilityTreeFormatterMac() {}

void AccessibilityTreeFormatterMac::AddDefaultFilters(
    std::vector<PropertyFilter>* property_filters) {
  static NSArray* default_attributes = [@[
    @"AXAutocompleteValue=*", @"AXDescription=*", @"AXRole=*", @"AXTitle=*",
    @"AXTitleUIElement=*", @"AXHelp=*", @"AXValue=*"
  ] retain];

  for (NSString* attribute : default_attributes) {
    AddPropertyFilter(property_filters, SysNSStringToUTF8(attribute));
  }

  if (show_ids()) {
    AddPropertyFilter(property_filters, "id");
  }
}

std::unique_ptr<base::DictionaryValue>
AccessibilityTreeFormatterMac::BuildAccessibilityTree(
    BrowserAccessibility* root) {
  DCHECK(root);

  BrowserAccessibilityCocoa* cocoa_root = ToBrowserAccessibilityCocoa(root);

  int counter = 0;
  LineIndexesMap line_indexes_map;
  RecursiveBuildLineIndexesMap(cocoa_root, &line_indexes_map, &counter);

  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
  RecursiveBuildAccessibilityTree(cocoa_root, line_indexes_map, dict.get());
  return dict;
}

std::unique_ptr<base::DictionaryValue>
AccessibilityTreeFormatterMac::BuildAccessibilityTreeForProcess(
    base::ProcessId pid) {
  NOTREACHED();
  return nullptr;
}

std::unique_ptr<base::DictionaryValue>
AccessibilityTreeFormatterMac::BuildAccessibilityTreeForWindow(
    gfx::AcceleratedWidget widget) {
  NOTREACHED();
  return nullptr;
}

std::unique_ptr<base::DictionaryValue>
AccessibilityTreeFormatterMac::BuildAccessibilityTreeForPattern(
    const base::StringPiece& pattern) {
  NOTREACHED();
  return nullptr;
}

void AccessibilityTreeFormatterMac::RecursiveBuildAccessibilityTree(
    const BrowserAccessibilityCocoa* cocoa_node,
    const LineIndexesMap& line_indexes_map,
    base::DictionaryValue* dict) {
  AddProperties(cocoa_node, line_indexes_map, dict);

  auto children = std::make_unique<base::ListValue>();
  for (BrowserAccessibilityCocoa* cocoa_child in [cocoa_node children]) {
    std::unique_ptr<base::DictionaryValue> child_dict(
        new base::DictionaryValue);
    RecursiveBuildAccessibilityTree(cocoa_child, line_indexes_map,
                                    child_dict.get());
    children->Append(std::move(child_dict));
  }
  dict->Set(kChildrenDictAttr, std::move(children));
}

void AccessibilityTreeFormatterMac::RecursiveBuildLineIndexesMap(
    const BrowserAccessibilityCocoa* cocoa_node,
    LineIndexesMap* line_indexes_map,
    int* counter) {
  line_indexes_map->insert({cocoa_node, ++(*counter)});
  for (BrowserAccessibilityCocoa* cocoa_child in [cocoa_node children]) {
    RecursiveBuildLineIndexesMap(cocoa_child, line_indexes_map, counter);
  }
}

void AccessibilityTreeFormatterMac::AddProperties(
    const BrowserAccessibilityCocoa* cocoa_node,
    const LineIndexesMap& line_indexes_map,
    base::Value* dict) {
  // DOM element id
  BrowserAccessibility* node = [cocoa_node owner];
  dict->SetKey("id", base::Value(base::NumberToString16(node->GetId())));

  for (NSString* supportedAttribute in
       [cocoa_node accessibilityAttributeNames]) {
    if (FilterPropertyName(SysNSStringToUTF16(supportedAttribute))) {
      id value = [cocoa_node accessibilityAttributeValue:supportedAttribute];
      if (value != nil) {
        dict->SetPath(SysNSStringToUTF8(supportedAttribute),
                      PopulateObject(value, line_indexes_map));
      }
    }
  }
  dict->SetPath(kPositionDictAttr, PopulatePosition(cocoa_node));
  dict->SetPath(kSizeDictAttr, PopulateSize(cocoa_node));
}

base::Value AccessibilityTreeFormatterMac::PopulateSize(
    const BrowserAccessibilityCocoa* cocoa_node) const {
  base::Value size(base::Value::Type::DICTIONARY);
  NSSize node_size = [[cocoa_node size] sizeValue];
  size.SetIntPath(kHeightDictAttr, static_cast<int>(node_size.height));
  size.SetIntPath(kWidthDictAttr, static_cast<int>(node_size.width));
  return size;
}

base::Value AccessibilityTreeFormatterMac::PopulatePosition(
    const BrowserAccessibilityCocoa* cocoa_node) const {
  BrowserAccessibility* node = [cocoa_node owner];
  BrowserAccessibilityManager* root_manager = node->manager()->GetRootManager();
  DCHECK(root_manager);

  // The NSAccessibility position of an object is in global coordinates and
  // based on the lower-left corner of the object. To make this easier and less
  // confusing, convert it to local window coordinates using the top-left
  // corner when dumping the position.
  BrowserAccessibility* root = root_manager->GetRoot();
  BrowserAccessibilityCocoa* cocoa_root = ToBrowserAccessibilityCocoa(root);
  NSPoint root_position = [[cocoa_root position] pointValue];
  NSSize root_size = [[cocoa_root size] sizeValue];
  int root_top = -static_cast<int>(root_position.y + root_size.height);
  int root_left = static_cast<int>(root_position.x);

  NSPoint node_position = [[cocoa_node position] pointValue];
  NSSize node_size = [[cocoa_node size] sizeValue];

  base::Value position(base::Value::Type::DICTIONARY);
  position.SetIntPath(kXCoordDictAttr,
                      static_cast<int>(node_position.x - root_left));
  position.SetIntPath(
      kYCoordDictAttr,
      static_cast<int>(-node_position.y - node_size.height - root_top));
  return position;
}

base::Value AccessibilityTreeFormatterMac::PopulateObject(
    id value,
    const LineIndexesMap& line_indexes_map) const {
  // NSArray
  if ([value isKindOfClass:[NSArray class]]) {
    return PopulateArray((NSArray*)value, line_indexes_map);
  }

  // NSRange
  if ([value isKindOfClass:[NSValue class]] &&
      0 == strcmp([value objCType], @encode(NSRange))) {
    return PopulateRange([value rangeValue]);
  }

  // Accessible object.
  if ([value isKindOfClass:[BrowserAccessibilityCocoa class]]) {
    int line_index = -1;
    if (line_indexes_map.find(value) != line_indexes_map.end()) {
      line_index = line_indexes_map.at(value);
    }
    return base::Value(":" + base::NumberToString(line_index));
  }

  // Scalar value.
  return base::Value(
      SysNSStringToUTF16([NSString stringWithFormat:@"%@", value]));
}

base::Value AccessibilityTreeFormatterMac::PopulateRange(
    NSRange node_range) const {
  base::Value range(base::Value::Type::DICTIONARY);
  range.SetIntPath(kRangeLocDictAttr, static_cast<int>(node_range.location));
  range.SetIntPath(kRangeLenDictAttr, static_cast<int>(node_range.length));
  return range;
}

base::Value AccessibilityTreeFormatterMac::PopulateArray(
    NSArray* node_array,
    const LineIndexesMap& line_indexes_map) const {
  base::Value list(base::Value::Type::LIST);
  for (NSUInteger i = 0; i < [node_array count]; i++)
    list.Append(PopulateObject([node_array objectAtIndex:i], line_indexes_map));
  return list;
}

base::string16 AccessibilityTreeFormatterMac::ProcessTreeForOutput(
    const base::DictionaryValue& dict,
    base::DictionaryValue* filtered_dict_result) {
  base::string16 error_value;
  if (dict.GetString("error", &error_value))
    return error_value;

  base::string16 line;

  // AXRole and AXSubrole have own formatting and should be listed upfront.
  std::string role_attr = SysNSStringToUTF8(NSAccessibilityRoleAttribute);
  const std::string* value = dict.FindStringPath(role_attr);
  if (value) {
    WriteAttribute(true, *value, &line);
  }
  std::string subrole_attr = SysNSStringToUTF8(NSAccessibilitySubroleAttribute);
  value = dict.FindStringPath(subrole_attr);
  if (value) {
    WriteAttribute(false,
                   StringPrintf("%s=%s", subrole_attr.c_str(), value->c_str()),
                   &line);
  }

  // Expose all other attributes.
  for (auto item : dict.DictItems()) {
    if (item.second.is_string() &&
        (item.first == role_attr || item.first == subrole_attr)) {
      continue;
    }

    // Special processing for position and size.
    if (item.first == kPositionDictAttr) {
      WriteAttribute(false,
                     FormatCoordinates(
                         base::Value::AsDictionaryValue(item.second),
                         kPositionDictAttr, kXCoordDictAttr, kYCoordDictAttr),
                     &line);
      continue;
    }
    if (item.first == kSizeDictAttr) {
      WriteAttribute(
          false,
          FormatCoordinates(base::Value::AsDictionaryValue(item.second),
                            kSizeDictAttr, kWidthDictAttr, kHeightDictAttr),
          &line);
      continue;
    }

    // Write formatted value.
    std::string formatted_value = FormatAttributeValue(item.second);
    WriteAttribute(
        false,
        StringPrintf("%s=%s", item.first.c_str(), formatted_value.c_str()),
        &line);
  }

  return line;
}

std::string AccessibilityTreeFormatterMac::FormatAttributeValue(
    const base::Value& value) {
  if (value.is_string()) {
    // Special handling for accessible object relations, which are encoded
    // as a line index the related accessible object is written at. The format
    // is :LINE_NUM.
    if (RE2::FullMatch(value.GetString(), "^:\\d+$")) {
      return value.GetString();
    }
    return "'" + value.GetString() + "'";
  }

  if (value.is_int()) {
    return base::NumberToString(value.GetInt());
  }

  if (value.is_list()) {
    std::string output;
    for (const auto& item : value.GetList()) {
      if (!output.empty()) {
        output += ", ";
      }
      output += FormatAttributeValue(item);
    }
    return "[" + output + "]";
  }

  if (value.is_dict()) {
    std::string output;
    for (const auto& item : value.DictItems()) {
      if (!output.empty()) {
        output += ", ";
      }
      output += item.first + ": " + FormatAttributeValue(item.second);
    }
    return "{" + output + "}";
  }
  return "";
}

base::FilePath::StringType
AccessibilityTreeFormatterMac::GetExpectedFileSuffix() {
  return FILE_PATH_LITERAL("-expected-mac.txt");
}

const string AccessibilityTreeFormatterMac::GetAllowEmptyString() {
  return "@MAC-ALLOW-EMPTY:";
}

const string AccessibilityTreeFormatterMac::GetAllowString() {
  return "@MAC-ALLOW:";
}

const string AccessibilityTreeFormatterMac::GetDenyString() {
  return "@MAC-DENY:";
}

const string AccessibilityTreeFormatterMac::GetDenyNodeString() {
  return "@MAC-DENY-NODE:";
}

}  // namespace content

#pragma clang diagnostic pop
