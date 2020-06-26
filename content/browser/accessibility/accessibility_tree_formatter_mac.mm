// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_formatter_base.h"

#import <Cocoa/Cocoa.h>

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

const char kSetKeyPrefixDictAttr[] = "_setkey_";
const char kConstValuePrefix[] = "_const_";
const char kNULLValue[] = "_const_NULL";
const char kFailedToParseArgsError[] = "_const_ERROR:FAILED_TO_PARSE_ARGS";

#define INT_FAIL(propnode, msg)                                  \
  LOG(ERROR) << "Failed to parse " << propnode.original_property \
             << " to Int: " << msg;                              \
  return nil;

#define INTARRAY_FAIL(propnode, msg)                             \
  LOG(ERROR) << "Failed to parse " << propnode.original_property \
             << " to IntArray: " << msg;                         \
  return nil;

#define NSRANGE_FAIL(propnode, msg)                              \
  LOG(ERROR) << "Failed to parse " << propnode.original_property \
             << " to NSRange: " << msg;                          \
  return nil;

#define UIELEMENT_FAIL(propnode, msg)                            \
  LOG(ERROR) << "Failed to parse " << propnode.original_property \
             << " to UIElement: " << msg;                        \
  return nil;

#define TEXTMARKER_FAIL(propnode, msg)                                         \
  LOG(ERROR) << "Failed to parse " << propnode.original_property               \
             << " to AXTextMarker: " << msg                                    \
             << ". Expected format: {anchor, offset, affinity}, where anchor " \
                "is :line_num, offset is integer, affinity is either down, "   \
                "up or none";                                                  \
  return nil;

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
  using LineIndexesMap =
      std::map<const gfx::NativeViewAccessible, base::string16>;

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

  // Helper class used to compute a parameter for a parameterized attribute
  // call. Can be either id or error. Similar to base::Optional, but allows nil
  // id as a valid value.
  class IdOrError {
   public:
    IdOrError() : value(nil), error(false) {}

    IdOrError& operator=(id other_value) {
      error = !other_value;
      value = other_value;
      return *this;
    }

    bool IsError() const { return error; }
    bool IsNotNil() const { return !!value; }
    constexpr const id& operator*() const& { return value; }

   private:
    id value;
    bool error;
  };

  IdOrError ParamByPropertyNode(const PropertyNode&,
                                const LineIndexesMap&) const;
  NSNumber* PropertyNodeToInt(const PropertyNode&) const;
  NSArray* PropertyNodeToIntArray(const PropertyNode&) const;
  NSValue* PropertyNodeToRange(const PropertyNode&) const;
  gfx::NativeViewAccessible PropertyNodeToUIElement(
      const PropertyNode&,
      const LineIndexesMap&) const;
  id PropertyNodeToTextMarker(const PropertyNode&, const LineIndexesMap&) const;

  base::Value PopulateSize(const BrowserAccessibilityCocoa*) const;
  base::Value PopulatePosition(const BrowserAccessibilityCocoa*) const;
  base::Value PopulateRange(NSRange) const;
  base::Value PopulateTextPosition(
      BrowserAccessibilityPosition::AXPositionInstance::pointer,
      const LineIndexesMap&) const;
  base::Value PopulateTextMarkerRange(id, const LineIndexesMap&) const;
  base::Value PopulateObject(id, const LineIndexesMap& line_indexes_map) const;
  base::Value PopulateArray(NSArray*,
                            const LineIndexesMap& line_indexes_map) const;

  std::string NodeToLineIndex(id, const LineIndexesMap&) const;
  gfx::NativeViewAccessible LineIndexToNode(
      const base::string16 line_index,
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
  const base::string16 line_index =
      base::string16(1, ':') + base::NumberToString16(++(*counter));
  line_indexes_map->insert({cocoa_node, line_index});
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

  base::string16 line_index = base::ASCIIToUTF16("-1");
  if (line_indexes_map.find(cocoa_node) != line_indexes_map.end()) {
    line_index = line_indexes_map.at(cocoa_node);
  }

  // Attributes
  for (NSString* supportedAttribute in
       [cocoa_node accessibilityAttributeNames]) {
    if (GetMatchingPropertyNode(line_index,
                                SysNSStringToUTF16(supportedAttribute))) {
      id value = [cocoa_node accessibilityAttributeValue:supportedAttribute];
      if (value != nil) {
        dict->SetPath(SysNSStringToUTF8(supportedAttribute),
                      PopulateObject(value, line_indexes_map));
      }
    }
  }

  // Parameterized attributes
  for (NSString* supportedAttribute in
       [cocoa_node accessibilityParameterizedAttributeNames]) {
    auto propnode = GetMatchingPropertyNode(
        line_index, SysNSStringToUTF16(supportedAttribute));
    IdOrError param = ParamByPropertyNode(propnode, line_indexes_map);
    if (param.IsError()) {
      dict->SetPath(base::UTF16ToUTF8(propnode.original_property),
                    base::Value(kFailedToParseArgsError));
      continue;
    }

    if (param.IsNotNil()) {
      id value = [cocoa_node accessibilityAttributeValue:supportedAttribute
                                            forParameter:*param];
      dict->SetPath(base::UTF16ToUTF8(propnode.original_property),
                    PopulateObject(value, line_indexes_map));
    }
  }

  // Position and size
  dict->SetPath(kPositionDictAttr, PopulatePosition(cocoa_node));
  dict->SetPath(kSizeDictAttr, PopulateSize(cocoa_node));
}

AccessibilityTreeFormatterMac::IdOrError
AccessibilityTreeFormatterMac::ParamByPropertyNode(
    const PropertyNode& property_node,
    const LineIndexesMap& line_indexes_map) const {
  IdOrError param;
  std::string property_name = base::UTF16ToASCII(property_node.name_or_value);

  if (property_name == "AXLineForIndex") {  // Int
    param = PropertyNodeToInt(property_node);
  } else if (property_name == "AXCellForColumnAndRow") {  // IntArray
    param = PropertyNodeToIntArray(property_node);
  } else if (property_name == "AXStringForRange") {  // NSRange
    param = PropertyNodeToRange(property_node);
  } else if (property_name == "AXIndexForChildUIElement") {  // UIElement
    param = PropertyNodeToUIElement(property_node, line_indexes_map);
  } else if (property_name == "AXIndexForTextMarker") {  // TextMarker
    param = PropertyNodeToTextMarker(property_node, line_indexes_map);
  }

  return param;
}

// NSNumber. Format: integer.
NSNumber* AccessibilityTreeFormatterMac::PropertyNodeToInt(
    const PropertyNode& propnode) const {
  if (propnode.parameters.size() != 1) {
    INT_FAIL(propnode, "single argument is expected")
  }

  const auto& intnode = propnode.parameters[0];
  base::Optional<int> param = intnode.AsInt();
  if (!param) {
    INT_FAIL(propnode, "not a number")
  }
  return [NSNumber numberWithInt:*param];
}

// NSArray of two NSNumber. Format: [integer, integer].
NSArray* AccessibilityTreeFormatterMac::PropertyNodeToIntArray(
    const PropertyNode& propnode) const {
  if (propnode.parameters.size() != 1) {
    INTARRAY_FAIL(propnode, "single argument is expected")
  }

  const auto& arraynode = propnode.parameters[0];
  if (arraynode.name_or_value != base::ASCIIToUTF16("[]")) {
    INTARRAY_FAIL(propnode, "not array")
  }

  NSMutableArray* array =
      [[NSMutableArray alloc] initWithCapacity:arraynode.parameters.size()];
  for (const auto& paramnode : arraynode.parameters) {
    base::Optional<int> param = paramnode.AsInt();
    if (!param) {
      INTARRAY_FAIL(propnode, paramnode.name_or_value +
                                  base::UTF8ToUTF16(" is not a number"))
    }
    [array addObject:@(*param)];
  }
  return array;
}

// NSRange. Format: {loc: integer, len: integer}.
NSValue* AccessibilityTreeFormatterMac::PropertyNodeToRange(
    const PropertyNode& propnode) const {
  if (propnode.parameters.size() != 1) {
    NSRANGE_FAIL(propnode, "single argument is expected")
  }

  const auto& dictnode = propnode.parameters[0];
  if (!dictnode.IsDict()) {
    NSRANGE_FAIL(propnode, "dictionary is expected")
  }

  base::Optional<int> loc = dictnode.FindIntKey("loc");
  if (!loc) {
    NSRANGE_FAIL(propnode, "no loc or loc is not a number")
  }

  base::Optional<int> len = dictnode.FindIntKey("len");
  if (!len) {
    NSRANGE_FAIL(propnode, "no len or len is not a number")
  }

  return [NSValue valueWithRange:NSMakeRange(*loc, *len)];
}

// UIElement. Format: :line_num.
gfx::NativeViewAccessible
AccessibilityTreeFormatterMac::PropertyNodeToUIElement(
    const PropertyNode& propnode,
    const LineIndexesMap& line_indexes_map) const {
  if (propnode.parameters.size() != 1) {
    UIELEMENT_FAIL(propnode, "single argument is expected")
  }

  gfx::NativeViewAccessible uielement =
      LineIndexToNode(propnode.parameters[0].name_or_value, line_indexes_map);
  if (!uielement) {
    UIELEMENT_FAIL(propnode, "no corresponding UIElement was found in the tree")
  }
  return uielement;
}

id AccessibilityTreeFormatterMac::PropertyNodeToTextMarker(
    const PropertyNode& propnode,
    const LineIndexesMap& line_indexes_map) const {
  if (propnode.parameters.size() != 1) {
    TEXTMARKER_FAIL(propnode, "single argument is expected")
  }

  const auto& tmnode = propnode.parameters[0];
  if (!tmnode.IsDict()) {
    TEXTMARKER_FAIL(propnode, "dictionary is expected")
  }
  if (tmnode.parameters.size() != 3) {
    TEXTMARKER_FAIL(propnode, "wrong number of dictionary elements")
  }

  BrowserAccessibilityCocoa* anchor_cocoa =
      LineIndexToNode(tmnode.parameters[0].name_or_value, line_indexes_map);
  if (!anchor_cocoa) {
    TEXTMARKER_FAIL(propnode, "1st argument: wrong anchor")
  }

  base::Optional<int> offset = tmnode.parameters[1].AsInt();
  if (!offset) {
    TEXTMARKER_FAIL(propnode, "2nd argument: wrong offset")
  }

  ax::mojom::TextAffinity affinity;
  const base::string16& affinity_str = tmnode.parameters[2].name_or_value;
  if (affinity_str == base::UTF8ToUTF16("none")) {
    affinity = ax::mojom::TextAffinity::kNone;
  } else if (affinity_str == base::UTF8ToUTF16("down")) {
    affinity = ax::mojom::TextAffinity::kDownstream;
  } else if (affinity_str == base::UTF8ToUTF16("up")) {
    affinity = ax::mojom::TextAffinity::kUpstream;
  } else {
    TEXTMARKER_FAIL(propnode, "3rd argument: wrong affinity")
  }

  return content::AXTextMarkerFrom(anchor_cocoa, *offset, affinity);
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
  if (value == nil) {
    return base::Value(kNULLValue);
  }

  // NSArray
  if ([value isKindOfClass:[NSArray class]]) {
    return PopulateArray((NSArray*)value, line_indexes_map);
  }

  // NSNumber
  if ([value isKindOfClass:[NSNumber class]]) {
    return base::Value([value intValue]);
  }

  // NSRange
  if ([value isKindOfClass:[NSValue class]] &&
      0 == strcmp([value objCType], @encode(NSRange))) {
    return PopulateRange([value rangeValue]);
  }

  // AXTextMarker
  if (content::IsAXTextMarker(value)) {
    return PopulateTextPosition(content::AXTextMarkerToPosition(value).get(),
                                line_indexes_map);
  }

  // AXTextMarkerRange
  if (content::IsAXTextMarkerRange(value)) {
    return PopulateTextMarkerRange(value, line_indexes_map);
  }

  // Accessible object
  if ([value isKindOfClass:[BrowserAccessibilityCocoa class]]) {
    return base::Value(NodeToLineIndex(value, line_indexes_map));
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

base::Value AccessibilityTreeFormatterMac::PopulateTextPosition(
    BrowserAccessibilityPosition::AXPositionInstance::pointer position,
    const LineIndexesMap& line_indexes_map) const {
  if (position->IsNullPosition()) {
    return base::Value(kNULLValue);
  }

  BrowserAccessibility* anchor = position->GetAnchor();
  BrowserAccessibilityCocoa* cocoa_anchor = ToBrowserAccessibilityCocoa(anchor);

  std::string affinity;
  switch (position->affinity()) {
    case ax::mojom::TextAffinity::kNone:
      affinity = "none";
      break;
    case ax::mojom::TextAffinity::kDownstream:
      affinity = "down";
      break;
    case ax::mojom::TextAffinity::kUpstream:
      affinity = "up";
      break;
  }

  base::Value set(base::Value::Type::DICTIONARY);
  const std::string setkey_prefix = kSetKeyPrefixDictAttr;
  set.SetStringPath(setkey_prefix + "index1_anchor",
                    NodeToLineIndex(cocoa_anchor, line_indexes_map));
  set.SetIntPath(setkey_prefix + "index2_offset", position->text_offset());
  set.SetStringPath(setkey_prefix + "index3_affinity",
                    kConstValuePrefix + affinity);
  return set;
}

base::Value AccessibilityTreeFormatterMac::PopulateTextMarkerRange(
    id object,
    const LineIndexesMap& line_indexes_map) const {
  auto range = content::AXTextMarkerRangeToRange(object);
  if (range.IsNull()) {
    return base::Value(kNULLValue);
  }

  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetPath("anchor",
               PopulateTextPosition(range.anchor(), line_indexes_map));
  dict.SetPath("focus", PopulateTextPosition(range.focus(), line_indexes_map));
  return dict;
}

base::Value AccessibilityTreeFormatterMac::PopulateArray(
    NSArray* node_array,
    const LineIndexesMap& line_indexes_map) const {
  base::Value list(base::Value::Type::LIST);
  for (NSUInteger i = 0; i < [node_array count]; i++)
    list.Append(PopulateObject([node_array objectAtIndex:i], line_indexes_map));
  return list;
}

std::string AccessibilityTreeFormatterMac::NodeToLineIndex(
    id cocoa_node,
    const LineIndexesMap& line_indexes_map) const {
  std::string line_index = ":unknown";
  auto index_iterator = line_indexes_map.find(cocoa_node);
  if (index_iterator != line_indexes_map.end()) {
    line_index = base::UTF16ToUTF8(index_iterator->second);
  }
  return kConstValuePrefix + line_index;
}

gfx::NativeViewAccessible AccessibilityTreeFormatterMac::LineIndexToNode(
    const base::string16 line_index,
    const LineIndexesMap& line_indexes_map) const {
  for (std::pair<const gfx::NativeViewAccessible, base::string16> item :
       line_indexes_map) {
    if (item.second == line_index) {
      return item.first;
    }
  }
  return nil;
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
  // String.
  if (value.is_string()) {
    // Special handling for constants which are exposed as is, i.e. with no
    // quotation marks.
    std::string const_prefix = kConstValuePrefix;
    if (base::StartsWith(value.GetString(), const_prefix,
                         base::CompareCase::SENSITIVE)) {
      return value.GetString().substr(const_prefix.length());
    }
    return "'" + value.GetString() + "'";
  }

  // Integer.
  if (value.is_int()) {
    return base::NumberToString(value.GetInt());
  }

  // List: exposed as [value1, ..., valueN];
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

  // Dictionary. Exposed as {key1: value1, ..., keyN: valueN}. Set-like
  // dictionary is exposed as {value1, ..., valueN}.
  if (value.is_dict()) {
    const std::string setkey_prefix(kSetKeyPrefixDictAttr);
    std::string output;
    for (const auto& item : value.DictItems()) {
      if (!output.empty()) {
        output += ", ";
      }
      // Special set-like dictionaries handling: keys are prefixed by
      // "_setkey_".
      if (base::StartsWith(item.first, setkey_prefix,
                           base::CompareCase::SENSITIVE)) {
        output += FormatAttributeValue(item.second);
      } else {
        output += item.first + ": " + FormatAttributeValue(item.second);
      }
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
