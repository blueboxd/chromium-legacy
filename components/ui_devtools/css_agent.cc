// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/css_agent.h"

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/ui_devtools/ui_element.h"

namespace ui_devtools {
namespace {

using namespace ui_devtools::protocol;

const char kHeight[] = "height";
const char kWidth[] = "width";
const char kX[] = "x";
const char kY[] = "y";
const char kVisibility[] = "visibility";

std::unique_ptr<CSS::SourceRange> BuildDefaultSourceRange() {
  // These tell the frontend where in the stylesheet a certain style
  // is located. Since we don't have stylesheets, this is all 0.
  // We need this because CSS fields are not editable unless
  // the range is provided.
  return CSS::SourceRange::create()
      .setStartLine(0)
      .setEndLine(0)
      .setStartColumn(0)
      .setEndColumn(0)
      .build();
}

std::unique_ptr<protocol::Array<int>> BuildDefaultMatchingSelectors() {
  auto matching_selectors = std::make_unique<protocol::Array<int>>();

  // Add index 0 to matching delectors array, so frontend uses the class mame
  // from the selectors array as the header for the properties section
  matching_selectors->emplace_back(0);
  return matching_selectors;
}

std::unique_ptr<CSS::CSSProperty> BuildCSSProperty(const std::string& name,
                                                   const std::string& value) {
  return CSS::CSSProperty::create()
      .setRange(BuildDefaultSourceRange())
      .setName(name)
      .setValue(value)
      .build();
}

std::unique_ptr<Array<CSS::CSSProperty>> BuildCSSProperties(
    const std::vector<UIElement::UIProperty>& properties_vector) {
  auto css_properties = std::make_unique<Array<protocol::CSS::CSSProperty>>();
  for (const auto& property : properties_vector) {
    css_properties->emplace_back(
        BuildCSSProperty(property.name_, property.value_));
  }
  return css_properties;
}

std::unique_ptr<CSS::CSSStyle> BuildCSSStyle(
    std::string stylesheet_uid,
    const std::vector<UIElement::UIProperty>& properties) {
  return protocol::CSS::CSSStyle::create()
      .setRange(BuildDefaultSourceRange())
      .setCssProperties(BuildCSSProperties(properties))
      .setShorthandEntries(
          std::make_unique<Array<protocol::CSS::ShorthandEntry>>())
      .setStyleSheetId(stylesheet_uid)
      .build();
}

std::unique_ptr<protocol::Array<protocol::CSS::Value>> BuildSelectors(
    const std::string& name) {
  auto selectors = std::make_unique<protocol::Array<protocol::CSS::Value>>();
  selectors->emplace_back(protocol::CSS::Value::create().setText(name).build());
  return selectors;
}

std::unique_ptr<protocol::CSS::SelectorList> BuildSelectorList(
    const std::string& name) {
  return protocol::CSS::SelectorList::create()
      .setSelectors(BuildSelectors(name))
      .build();
}

std::unique_ptr<protocol::CSS::CSSRule> BuildCSSRule(
    std::string stylesheet_uid,
    const UIElement::ClassProperties& class_properties) {
  return protocol::CSS::CSSRule::create()
      .setSelectorList(BuildSelectorList(class_properties.class_name_))
      .setStyle(BuildCSSStyle(stylesheet_uid, class_properties.properties_))
      .build();
}

std::vector<UIElement::ClassProperties> GetClassPropertiesWithBounds(
    UIElement* ui_element) {
  std::vector<UIElement::ClassProperties> properties_vector =
      ui_element->GetCustomPropertiesForMatchedStyle();

  // If GetCustomPropertiesForMatchedStyle not overridden to return custom
  // properties, populate vector with bounds properties.
  if (properties_vector.empty()) {
    gfx::Rect bounds;
    ui_element->GetBounds(&bounds);
    std::vector<UIElement::UIProperty> bound_properties;
    bound_properties.emplace_back(kX, base::NumberToString(bounds.x()));
    bound_properties.emplace_back(kY, base::NumberToString(bounds.y()));
    bound_properties.emplace_back(kWidth, base::NumberToString(bounds.width()));
    bound_properties.emplace_back(kHeight,
                                  base::NumberToString(bounds.height()));
    if (ui_element->type() != VIEW) {
      bool visible;
      ui_element->GetVisible(&visible);
      bound_properties.emplace_back(kVisibility, visible ? "true" : "false");
    }
    properties_vector.emplace_back(ui_element->GetTypeName(), bound_properties);
  }

  // Set base stylesheet ID to the last index in the vector, so when bounds
  // properties are modified, CSSAgent can update and return the right
  // properties section
  ui_element->SetBaseStylesheetId(properties_vector.size() - 1);
  return properties_vector;
}

std::string BuildStylesheetUId(int node_id, int stylesheet_id) {
  return base::NumberToString(node_id) + "_" +
         base::NumberToString(stylesheet_id);
}

Response NodeNotFoundError(int node_id) {
  return Response::Error("Node with id=" + std::to_string(node_id) +
                         " not found");
}

Response ParseProperties(const std::string& style_text,
                         gfx::Rect* bounds,
                         bool* visible) {
  std::vector<std::string> tokens = base::SplitString(
      style_text, ":;", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (size_t i = 0; i < tokens.size() - 1; i += 2) {
    const std::string& property = tokens.at(i);
    int value;
    if (!base::StringToInt(tokens.at(i + 1), &value))
      return Response::Error("Unable to parse value for property=" + property);

    if (property == kHeight)
      bounds->set_height(std::max(0, value));
    else if (property == kWidth)
      bounds->set_width(std::max(0, value));
    else if (property == kX)
      bounds->set_x(value);
    else if (property == kY)
      bounds->set_y(value);
    else if (property == kVisibility)
      *visible = std::max(0, value) == 1;
    else
      return Response::Error("Unsupported property=" + property);
  }
  return Response::OK();
}

}  // namespace

CSSAgent::CSSAgent(DOMAgent* dom_agent) : dom_agent_(dom_agent) {
  DCHECK(dom_agent_);
}

CSSAgent::~CSSAgent() {
  disable();
}

Response CSSAgent::enable() {
  dom_agent_->AddObserver(this);
  return Response::OK();
}

Response CSSAgent::disable() {
  dom_agent_->RemoveObserver(this);
  return Response::OK();
}

Response CSSAgent::getMatchedStylesForNode(
    int node_id,
    protocol::Maybe<protocol::Array<protocol::CSS::RuleMatch>>*
        matched_css_rules) {
  UIElement* ui_element = dom_agent_->GetElementFromNodeId(node_id);
  if (!ui_element)
    return NodeNotFoundError(node_id);
  *matched_css_rules = BuildMatchedStyles(ui_element);
  return Response::OK();
}

Response CSSAgent::setStyleTexts(
    std::unique_ptr<Array<CSS::StyleDeclarationEdit>> edits,
    std::unique_ptr<Array<CSS::CSSStyle>>* result) {
  auto updated_styles = std::make_unique<Array<CSS::CSSStyle>>();
  for (const auto& edit : *edits) {
    int node_id;
    int stylesheet_id;

    std::vector<std::string> ids =
        base::SplitString(edit->getStyleSheetId(), "_", base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);
    if (ids.size() < 2 || !base::StringToInt(ids[0], &node_id) ||
        !base::StringToInt(ids[1], &stylesheet_id))
      return Response::Error("Invalid stylesheet id");

    UIElement* ui_element = dom_agent_->GetElementFromNodeId(node_id);

    if (!ui_element)
      return Response::Error("Node id not found");
    // Handle setting properties from metadata for View.
    if (ui_element->type() == VIEW)
      ui_element->SetPropertiesFromString(edit->getText());

    gfx::Rect updated_bounds;
    bool visible = false;
    if (!GetPropertiesForUIElement(ui_element, &updated_bounds, &visible))
      return NodeNotFoundError(node_id);

    Response response(
        ParseProperties(edit->getText(), &updated_bounds, &visible));
    if (!response.isSuccess())
      return response;

    if (!SetPropertiesForUIElement(ui_element, updated_bounds, visible))
      return NodeNotFoundError(node_id);

    updated_styles->emplace_back(BuildCSSStyle(
        edit->getStyleSheetId(), GetClassPropertiesWithBounds(ui_element)
                                     .at(stylesheet_id)
                                     .properties_));
  }
  *result = std::move(updated_styles);
  return Response::OK();
}

void CSSAgent::OnElementBoundsChanged(UIElement* ui_element) {
  InvalidateStyleSheet(ui_element);
}

void CSSAgent::InvalidateStyleSheet(UIElement* ui_element) {
  // The stylesheetId for each node is equivalent to a string of its
  // node_id + "_" + index of CSS::RuleMatch in vector.
  frontend()->styleSheetChanged(BuildStylesheetUId(
      ui_element->node_id(), ui_element->GetBaseStylesheetId()));
}

bool CSSAgent::GetPropertiesForUIElement(UIElement* ui_element,
                                         gfx::Rect* bounds,
                                         bool* visible) {
  if (ui_element) {
    ui_element->GetBounds(bounds);
    if (ui_element->type() != VIEW)
      ui_element->GetVisible(visible);
    return true;
  }
  return false;
}

bool CSSAgent::SetPropertiesForUIElement(UIElement* ui_element,
                                         const gfx::Rect& bounds,
                                         bool visible) {
  if (ui_element) {
    ui_element->SetBounds(bounds);
    ui_element->SetVisible(visible);
    return true;
  }
  return false;
}

std::unique_ptr<protocol::Array<protocol::CSS::RuleMatch>>
CSSAgent::BuildMatchedStyles(UIElement* ui_element) {
  auto result = std::make_unique<protocol::Array<protocol::CSS::RuleMatch>>();
  std::vector<UIElement::ClassProperties> properties_vector =
      GetClassPropertiesWithBounds(ui_element);

  for (size_t i = 0; i < properties_vector.size(); i++) {
    result->emplace_back(
        protocol::CSS::RuleMatch::create()
            .setRule(BuildCSSRule(BuildStylesheetUId(ui_element->node_id(), i),
                                  properties_vector[i]))
            .setMatchingSelectors(BuildDefaultMatchingSelectors())
            .build());
  }

  return result;
}

}  // namespace ui_devtools
