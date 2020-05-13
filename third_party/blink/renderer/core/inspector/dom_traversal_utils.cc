#include "third_party/blink/renderer/core/inspector/dom_traversal_utils.h"

#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"

namespace blink {
namespace dom_traversal_utils {

Node* FirstChild(const Node& node, bool include_user_agent_shadow_tree) {
  DCHECK(include_user_agent_shadow_tree || !node.IsInUserAgentShadowRoot());
  if (!include_user_agent_shadow_tree) {
    ShadowRoot* shadow_root = node.GetShadowRoot();
    if (shadow_root && shadow_root->GetType() == ShadowRootType::kUserAgent) {
      Node* child = node.firstChild();
      while (child && !child->CanParticipateInFlatTree())
        child = child->nextSibling();
      return child;
    }
  }
  return FlatTreeTraversal::FirstChild(node);
}

// static
bool HasChildren(const Node& node, bool include_user_agent_shadow_tree) {
  return FirstChild(node, include_user_agent_shadow_tree);
}

// static
Node* NextSibling(const Node& node, bool include_user_agent_shadow_tree) {
  DCHECK(include_user_agent_shadow_tree || !node.IsInUserAgentShadowRoot());
  if (!include_user_agent_shadow_tree) {
    if (node.ParentElementShadowRoot() &&
        node.ParentElementShadowRoot()->GetType() ==
            ShadowRootType::kUserAgent) {
      Node* sibling = node.nextSibling();
      while (sibling && !sibling->CanParticipateInFlatTree())
        sibling = sibling->nextSibling();
      return sibling;
    }
  }
  return FlatTreeTraversal::NextSibling(node);
}

}  // namespace dom_traversal_utils
}  // namespace blink
