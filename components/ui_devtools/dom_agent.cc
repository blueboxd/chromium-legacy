// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/dom_agent.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/containers/adapters.h"
#include "components/ui_devtools/devtools_server.h"
#include "components/ui_devtools/root_element.h"
#include "components/ui_devtools/ui_element.h"

namespace {

std::string CreateIdentifier() {
  static int last_used_identifier;
  return base::NumberToString(++last_used_identifier);
}

}  // namespace

namespace ui_devtools {

using ui_devtools::protocol::Array;
using ui_devtools::protocol::DOM::Node;
using ui_devtools::protocol::Response;

DOMAgent::DOMAgent() {}

DOMAgent::~DOMAgent() {
  Reset();
}

Response DOMAgent::disable() {
  Reset();
  return Response::OK();
}

Response DOMAgent::getDocument(std::unique_ptr<Node>* out_root) {
  element_root_->ResetNodeId();
  *out_root = BuildInitialTree();
  is_document_created_ = true;
  return Response::OK();
}

Response DOMAgent::pushNodesByBackendIdsToFrontend(
    std::unique_ptr<protocol::Array<int>> backend_node_ids,
    std::unique_ptr<protocol::Array<int>>* result) {
  *result = std::move(backend_node_ids);
  return Response::OK();
}

void DOMAgent::OnUIElementAdded(UIElement* parent, UIElement* child) {
  // When parent is null, only need to update |node_id_to_ui_element_|.
  if (!parent) {
    node_id_to_ui_element_[child->node_id()] = child;
    return;
  }

  DCHECK(node_id_to_ui_element_.count(parent->node_id()));

  auto* current_parent = parent;
  while (current_parent) {
    if (current_parent->is_updating()) {
      // One of the parents is updating, so no need to update here.
      return;
    }
    current_parent = current_parent->parent();
  }

  child->set_is_updating(true);

  const auto& children = parent->children();
  auto iter = std::find(children.begin(), children.end(), child);
  int prev_node_id =
      (iter == children.begin()) ? 0 : (*std::prev(iter))->node_id();
  frontend()->childNodeInserted(parent->node_id(), prev_node_id,
                                BuildTreeForUIElement(child));

  child->set_is_updating(false);
  for (auto& observer : observers_)
    observer.OnElementAdded(child);
}

void DOMAgent::OnUIElementReordered(UIElement* parent, UIElement* child) {
  DCHECK(node_id_to_ui_element_.count(parent->node_id()));

  const auto& children = parent->children();
  auto iter = std::find(children.begin(), children.end(), child);
  int prev_node_id =
      (iter == children.begin()) ? 0 : (*std::prev(iter))->node_id();
  RemoveDomNode(child);
  frontend()->childNodeInserted(parent->node_id(), prev_node_id,
                                BuildDomNodeFromUIElement(child));
}

void DOMAgent::OnUIElementRemoved(UIElement* ui_element) {
  DCHECK(node_id_to_ui_element_.count(ui_element->node_id()));

  RemoveDomNode(ui_element);
  node_id_to_ui_element_.erase(ui_element->node_id());
}

void DOMAgent::OnUIElementBoundsChanged(UIElement* ui_element) {
  for (auto& observer : observers_)
    observer.OnElementBoundsChanged(ui_element);
}

void DOMAgent::AddObserver(DOMAgentObserver* observer) {
  observers_.AddObserver(observer);
}

void DOMAgent::RemoveObserver(DOMAgentObserver* observer) {
  observers_.RemoveObserver(observer);
}

UIElement* DOMAgent::GetElementFromNodeId(int node_id) const {
  auto it = node_id_to_ui_element_.find(node_id);
  if (it != node_id_to_ui_element_.end())
    return it->second;
  return nullptr;
}

int DOMAgent::GetParentIdOfNodeId(int node_id) const {
  DCHECK(node_id_to_ui_element_.count(node_id));
  const UIElement* element = node_id_to_ui_element_.at(node_id);
  if (element->parent() && element->parent() != element_root_.get())
    return element->parent()->node_id();
  return 0;
}

std::unique_ptr<Node> DOMAgent::BuildNode(
    const std::string& name,
    std::unique_ptr<std::vector<std::string>> attributes,
    std::unique_ptr<Array<Node>> children,
    int node_ids) {
  constexpr int kDomElementNodeType = 1;
  std::unique_ptr<Node> node = Node::create()
                                   .setNodeId(node_ids)
                                   .setBackendNodeId(node_ids)
                                   .setNodeName(name)
                                   .setNodeType(kDomElementNodeType)
                                   .setAttributes(std::move(attributes))
                                   .build();
  node->setChildNodeCount(static_cast<int>(children->size()));
  node->setChildren(std::move(children));
  return node;
}

std::unique_ptr<Node> DOMAgent::BuildDomNodeFromUIElement(UIElement* root) {
  auto children = std::make_unique<protocol::Array<Node>>();
  for (auto* it : root->children())
    children->emplace_back(BuildDomNodeFromUIElement(it));

  return BuildNode(
      root->GetTypeName(),
      std::make_unique<std::vector<std::string>>(root->GetAttributes()),
      std::move(children), root->node_id());
}

std::unique_ptr<Node> DOMAgent::BuildInitialTree() {
  auto children = std::make_unique<protocol::Array<Node>>();

  element_root_ = std::make_unique<RootElement>(this);
  element_root_->set_is_updating(true);

  for (auto* child : CreateChildrenForRoot()) {
    children->emplace_back(BuildTreeForUIElement(child));
    element_root_->AddChild(child);
  }
  std::unique_ptr<Node> root_node =
      BuildNode("root", nullptr, std::move(children), element_root_->node_id());
  element_root_->set_is_updating(false);
  return root_node;
}

void DOMAgent::OnElementBoundsChanged(UIElement* ui_element) {
  for (auto& observer : observers_)
    observer.OnElementBoundsChanged(ui_element);
}

void DOMAgent::RemoveDomNode(UIElement* ui_element) {
  for (auto* child_element : ui_element->children())
    RemoveDomNode(child_element);
  frontend()->childNodeRemoved(ui_element->parent()->node_id(),
                               ui_element->node_id());
}

void DOMAgent::Reset() {
  element_root_.reset();
  node_id_to_ui_element_.clear();
  observers_.Clear();
  is_document_created_ = false;
  search_results_.clear();
}

// code is based on InspectorDOMAgent::performSearch() from
// src/third_party/blink/renderer/core/inspector/inspector_dom_agent.cc
Response DOMAgent::performSearch(
    const protocol::String& whitespace_trimmed_query,
    protocol::Maybe<bool> optional_include_user_agent_shadow_dom,
    protocol::String* search_id,
    int* result_count) {
  protocol::String query = whitespace_trimmed_query;
  std::transform(query.begin(), query.end(), query.begin(), ::tolower);

  unsigned query_length = query.length();
  bool start_tag_found = !query.find('<');
  bool end_tag_found = query.rfind('>') + 1 == query_length;
  bool start_quote_found = !query.find('"');
  bool end_quote_found = query.rfind('"') + 1 == query_length;
  bool exact_attribute_match = start_quote_found && end_quote_found;

  protocol::String tag_name_query = query;
  protocol::String attribute_query = query;
  if (start_tag_found)
    tag_name_query = tag_name_query.substr(1, tag_name_query.length() - 1);
  if (end_tag_found)
    tag_name_query = tag_name_query.substr(0, tag_name_query.length() - 1);
  if (start_quote_found)
    attribute_query = attribute_query.substr(1, attribute_query.length() - 1);
  if (end_quote_found)
    attribute_query = attribute_query.substr(0, attribute_query.length() - 1);

  std::vector<int> result_collector;
  std::vector<UIElement*> stack;

  // root node from element_root() is not a real node from DOM tree
  // the children of the root node are the 'actual' roots of the DOM tree
  UIElement* root = element_root();
  std::vector<UIElement*> root_list = root->children();
  DCHECK(root_list.size());
  // Children are accessed from bottom to top. So iterate backwards.
  for (auto* root : base::Reversed(root_list))
    stack.push_back(root);

  // Manual plain text search. DFS traversal.
  while (!stack.empty()) {
    UIElement* node = stack.back();
    stack.pop_back();
    protocol::String node_name = node->GetTypeName();
    std::transform(node_name.begin(), node_name.end(), node_name.begin(),
                   ::tolower);

    std::vector<UIElement*> children_array = node->children();
    // Children are accessed from bottom to top. So iterate backwards.
    for (auto* child : base::Reversed(children_array))
      stack.push_back(child);

    if (node_name.find(query) != protocol::String::npos ||
        node_name == tag_name_query) {
      result_collector.push_back(node->node_id());
      continue;
    }

    for (std::string& data : node->GetAttributes()) {
      std::transform(data.begin(), data.end(), data.begin(), ::tolower);
      if (data.find(query) != protocol::String::npos) {
        result_collector.push_back(node->node_id());
        break;
      }
      if (data.find(attribute_query) != protocol::String::npos) {
        if (!exact_attribute_match ||
            data.length() == attribute_query.length()) {
          result_collector.push_back(node->node_id());
        }
        break;
      }
    }
  }

  *search_id = CreateIdentifier();
  *result_count = result_collector.size();
  search_results_.insert(
      std::make_pair(*search_id, std::move(result_collector)));
  return Response::OK();
}

Response DOMAgent::getSearchResults(
    const protocol::String& search_id,
    int from_index,
    int to_index,
    std::unique_ptr<protocol::Array<int>>* node_ids) {
  SearchResults::iterator it = search_results_.find(search_id);
  if (it == search_results_.end())
    return Response::Error("No search session with given id found");

  int size = it->second.size();
  if (from_index < 0 || to_index > size || from_index >= to_index)
    return Response::Error("Invalid search result range");

  *node_ids = std::make_unique<protocol::Array<int>>();
  for (int i = from_index; i < to_index; ++i)
    (*node_ids)->emplace_back((it->second)[i]);
  return Response::OK();
}

Response DOMAgent::discardSearchResults(const protocol::String& search_id) {
  search_results_.erase(search_id);
  return Response::OK();
}

}  // namespace ui_devtools
