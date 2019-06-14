// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/frame_node_impl.h"

#include "chrome/browser/performance_manager/graph/graph_impl.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "chrome/browser/performance_manager/performance_manager_clock.h"

namespace performance_manager {

FrameNodeImpl::FrameNodeImpl(GraphImpl* graph,
                             ProcessNodeImpl* process_node,
                             PageNodeImpl* page_node,
                             FrameNodeImpl* parent_frame_node,
                             int frame_tree_node_id,
                             const base::UnguessableToken& dev_tools_token,
                             int32_t browsing_instance_id,
                             int32_t site_instance_id)
    : TypedNodeBase(graph),
      binding_(this),
      parent_frame_node_(parent_frame_node),
      page_node_(page_node),
      process_node_(process_node),
      frame_tree_node_id_(frame_tree_node_id),
      dev_tools_token_(dev_tools_token),
      browsing_instance_id_(browsing_instance_id),
      site_instance_id_(site_instance_id) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK(process_node);
  DCHECK(page_node);
}

FrameNodeImpl::~FrameNodeImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void FrameNodeImpl::Bind(
    resource_coordinator::mojom::DocumentCoordinationUnitRequest request) {
  // It is possible to receive a DocumentCoordinationUnitRequest when |binding_|
  // is already bound in these cases:
  // - Navigation from the initial empty document to the first real document.
  // - Navigation rejected by RenderFrameHostImpl::ValidateDidCommitParams().
  // See discussion:
  // https://chromium-review.googlesource.com/c/chromium/src/+/1572459/6#message-bd31f3e73f96bd9f7721be81ba6ac0076d053147
  if (binding_.is_bound())
    binding_.Close();

  binding_.Bind(std::move(request));
}

void FrameNodeImpl::SetNetworkAlmostIdle() {
  document_.network_almost_idle.SetAndMaybeNotify(this, true);
}

void FrameNodeImpl::SetLifecycleState(
    resource_coordinator::mojom::LifecycleState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  lifecycle_state_.SetAndMaybeNotify(this, state);
}

void FrameNodeImpl::SetHasNonEmptyBeforeUnload(bool has_nonempty_beforeunload) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  document_.has_nonempty_beforeunload = has_nonempty_beforeunload;
}

void FrameNodeImpl::SetInterventionPolicy(
    resource_coordinator::mojom::PolicyControlledIntervention intervention,
    resource_coordinator::mojom::InterventionPolicy policy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  size_t i = static_cast<size_t>(intervention);
  DCHECK_LT(i, base::size(intervention_policy_));

  // This can only be called to set a policy, but not to revert a policy to the
  // unset state.
  DCHECK_NE(resource_coordinator::mojom::InterventionPolicy::kUnknown, policy);

  // We expect intervention policies to be initially set in order, and rely on
  // that as a synchronization primitive. Ensure this is the case.
  DCHECK(i == 0 ||
         intervention_policy_[i - 1] !=
             resource_coordinator::mojom::InterventionPolicy::kUnknown);

  if (policy == intervention_policy_[i])
    return;
  // Only notify of actual changes.
  resource_coordinator::mojom::InterventionPolicy old_policy =
      intervention_policy_[i];
  intervention_policy_[i] = policy;
  page_node_->OnFrameInterventionPolicyChanged(this, intervention, old_policy,
                                               policy);
}

void FrameNodeImpl::SetIsAdFrame() {
  is_ad_frame_ = true;
}

void FrameNodeImpl::OnNonPersistentNotificationCreated() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observers())
    observer.OnNonPersistentNotificationCreated(this);
  for (auto* observer : GetObservers())
    observer->OnNonPersistentNotificationCreated(this);
}

bool FrameNodeImpl::IsMainFrame() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !parent_frame_node_;
}

bool FrameNodeImpl::AreAllInterventionPoliciesSet() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The convention is that policies are first set en masse, in order. So if
  // the last policy is set then they are all considered to be set. Check this
  // in DEBUG builds.
#if DCHECK_IS_ON()
  bool seen_unset_policy = false;
  for (size_t i = 0; i < base::size(intervention_policy_); ++i) {
    if (!seen_unset_policy) {
      seen_unset_policy =
          intervention_policy_[i] !=
          resource_coordinator::mojom::InterventionPolicy::kUnknown;
    } else {
      // Once a first unset policy is seen, all subsequent policies must be
      // unset.
      DCHECK_NE(resource_coordinator::mojom::InterventionPolicy::kUnknown,
                intervention_policy_[i]);
    }
  }
#endif

  return intervention_policy_[base::size(intervention_policy_) - 1] !=
         resource_coordinator::mojom::InterventionPolicy::kUnknown;
}

FrameNodeImpl* FrameNodeImpl::parent_frame_node() const {
  return parent_frame_node_;
}

PageNodeImpl* FrameNodeImpl::page_node() const {
  return page_node_;
}

ProcessNodeImpl* FrameNodeImpl::process_node() const {
  return process_node_;
}

int FrameNodeImpl::frame_tree_node_id() const {
  return frame_tree_node_id_;
}

const base::UnguessableToken& FrameNodeImpl::dev_tools_token() const {
  return dev_tools_token_;
}

int32_t FrameNodeImpl::browsing_instance_id() const {
  return browsing_instance_id_;
}

int32_t FrameNodeImpl::site_instance_id() const {
  return site_instance_id_;
}

const base::flat_set<FrameNodeImpl*>& FrameNodeImpl::child_frame_nodes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return child_frame_nodes_;
}

resource_coordinator::mojom::LifecycleState FrameNodeImpl::lifecycle_state()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return lifecycle_state_.value();
}

bool FrameNodeImpl::has_nonempty_beforeunload() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return document_.has_nonempty_beforeunload;
}

const GURL& FrameNodeImpl::url() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return document_.url.value();
}

bool FrameNodeImpl::is_current() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_current_.value();
}

bool FrameNodeImpl::network_almost_idle() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return document_.network_almost_idle.value();
}

bool FrameNodeImpl::is_ad_frame() const {
  return is_ad_frame_;
}

void FrameNodeImpl::SetIsCurrent(bool is_current) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_current_.SetAndMaybeNotify(this, is_current);

#if DCHECK_IS_ON()
  // We maintain an invariant that of all sibling nodes with the same
  // |frame_tree_node_id|, at most one may be current.
  if (is_current) {
    const base::flat_set<FrameNodeImpl*>* siblings = nullptr;
    if (parent_frame_node_) {
      siblings = &parent_frame_node_->child_frame_nodes();
    } else {
      siblings = &page_node_->main_frame_nodes();
    }

    size_t current_siblings = 0;
    for (auto* frame : *siblings) {
      if (frame->frame_tree_node_id() == frame_tree_node_id_ &&
          frame->is_current()) {
        ++current_siblings;
      }
    }
    DCHECK_EQ(1u, current_siblings);
  }
#endif
}

void FrameNodeImpl::OnNavigationCommitted(const GURL& url, bool same_document) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (same_document) {
    document_.url.SetAndMaybeNotify(this, url);
    return;
  }

  // Close |binding_| to ensure that messages queued by the previous document
  // before the navigation commit are dropped.
  //
  // Note: It is guaranteed that |binding_| isn't yet bound to the new document.
  //       This is important because it would be incorrect to close the new
  //       document's binding.
  //
  //       Renderer: blink::DocumentLoader::DidCommitNavigation
  //                   ... content::RenderFrameImpl::DidCommitProvisionalLoad
  //                     ... mojom::FrameHost::DidCommitProvisionalLoad
  //       Browser:  RenderFrameHostImpl::DidCommitNavigation
  //                   Bind the new document's interface provider [A]
  //                   PMTabHelper::DidFinishNavigation
  //                     (async) FrameNodeImpl::OnNavigationCommitted [B]
  //       Renderer: Request DocumentCoordinationUnit interface
  //       Browser:  PMTabHelper::OnInterfaceRequestFromFrame [C]
  //                   (async) FrameNodeImpl::Bind [D]
  //
  //       A happens before C, because no interface request can be processed
  //       before the interface provider is bound. A posts B to PM sequence and
  //       C posts D to PM sequence, therefore B happens before D.
  binding_.Close();

  // Reset properties.
  document_.Reset(this, url);
}

void FrameNodeImpl::SetAllInterventionPoliciesForTesting(
    resource_coordinator::mojom::InterventionPolicy policy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (size_t i = 0; i < base::size(intervention_policy_); ++i) {
    SetInterventionPolicy(
        static_cast<resource_coordinator::mojom::PolicyControlledIntervention>(
            i),
        policy);
  }
}

const FrameNode* FrameNodeImpl::GetParentFrameNode() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return parent_frame_node();
}

const PageNode* FrameNodeImpl::GetPageNode() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return page_node();
}

const ProcessNode* FrameNodeImpl::GetProcessNode() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return process_node();
}

int FrameNodeImpl::GetFrameTreeNodeId() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return frame_tree_node_id();
}

const base::UnguessableToken& FrameNodeImpl::GetDevToolsToken() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return dev_tools_token();
}

int32_t FrameNodeImpl::GetBrowsingInstanceId() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return browsing_instance_id();
}

int32_t FrameNodeImpl::GetSiteInstanceId() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return site_instance_id();
}

const base::flat_set<const FrameNode*> FrameNodeImpl::GetChildFrameNodes()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::flat_set<const FrameNode*> children;
  for (auto* child : child_frame_nodes())
    children.insert(static_cast<const FrameNode*>(child));
  DCHECK_EQ(children.size(), child_frame_nodes().size());
  return children;
}

FrameNodeImpl::LifecycleState FrameNodeImpl::GetLifecycleState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return lifecycle_state();
}

bool FrameNodeImpl::HasNonemptyBeforeUnload() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return has_nonempty_beforeunload();
}

const GURL& FrameNodeImpl::GetURL() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return url();
}

bool FrameNodeImpl::IsCurrent() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_current();
}

bool FrameNodeImpl::GetNetworkAlmostIdle() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return network_almost_idle();
}

bool FrameNodeImpl::IsAdFrame() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_ad_frame();
}

void FrameNodeImpl::AddChildFrame(FrameNodeImpl* child_frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(child_frame_node);
  DCHECK_EQ(this, child_frame_node->parent_frame_node());
  DCHECK_NE(this, child_frame_node);
  DCHECK(graph()->NodeInGraph(child_frame_node));
  DCHECK(!HasFrameNodeInAncestors(child_frame_node) &&
         !child_frame_node->HasFrameNodeInDescendants(this));

  bool inserted = child_frame_nodes_.insert(child_frame_node).second;
  DCHECK(inserted);
}

void FrameNodeImpl::RemoveChildFrame(FrameNodeImpl* child_frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(child_frame_node);
  DCHECK_EQ(this, child_frame_node->parent_frame_node());
  DCHECK_NE(this, child_frame_node);
  DCHECK(graph()->NodeInGraph(child_frame_node));

  size_t removed = child_frame_nodes_.erase(child_frame_node);
  DCHECK_EQ(1u, removed);
}

void FrameNodeImpl::JoinGraph() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (size_t i = 0; i < base::size(intervention_policy_); ++i)
    intervention_policy_[i] =
        resource_coordinator::mojom::InterventionPolicy::kUnknown;

  // Wire this up to the other nodes in the graph.
  if (parent_frame_node_)
    parent_frame_node_->AddChildFrame(this);
  page_node_->AddFrame(this);
  process_node_->AddFrame(this);

  NodeBase::JoinGraph();
}

void FrameNodeImpl::LeaveGraph() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NodeBase::LeaveGraph();

  DCHECK(child_frame_nodes_.empty());

  // Leave the page.
  DCHECK(graph()->NodeInGraph(page_node_));
  page_node_->RemoveFrame(this);

  // Leave the frame hierarchy.
  if (parent_frame_node_) {
    DCHECK(graph()->NodeInGraph(parent_frame_node_));
    parent_frame_node_->RemoveChildFrame(this);
  }

  // And leave the process.
  DCHECK(graph()->NodeInGraph(process_node_));
  process_node_->RemoveFrame(this);
}

bool FrameNodeImpl::HasFrameNodeInAncestors(FrameNodeImpl* frame_node) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (parent_frame_node_ == frame_node ||
      (parent_frame_node_ &&
       parent_frame_node_->HasFrameNodeInAncestors(frame_node))) {
    return true;
  }
  return false;
}

bool FrameNodeImpl::HasFrameNodeInDescendants(FrameNodeImpl* frame_node) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (FrameNodeImpl* child : child_frame_nodes_) {
    if (child == frame_node || child->HasFrameNodeInDescendants(frame_node)) {
      return true;
    }
  }
  return false;
}

FrameNodeImpl::DocumentProperties::DocumentProperties() = default;
FrameNodeImpl::DocumentProperties::~DocumentProperties() = default;

void FrameNodeImpl::DocumentProperties::Reset(FrameNodeImpl* frame_node,
                                              const GURL& url_in) {
  url.SetAndMaybeNotify(frame_node, url_in);
  has_nonempty_beforeunload = false;
  // Network is busy on navigation.
  network_almost_idle.SetAndMaybeNotify(frame_node, false);
}

}  // namespace performance_manager
