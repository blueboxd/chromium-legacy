// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_view.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_actions_bar_bubble_views.h"
#include "ui/views/widget/widget_observer.h"

ExtensionsToolbarContainer::ExtensionsToolbarContainer(Browser* browser)
    : ToolbarIconContainerView(/*uses_highlight=*/true),
      browser_(browser),
      model_(ToolbarActionsModel::Get(browser_->profile())),
      model_observer_(this),
      extensions_button_(new ExtensionsToolbarButton(browser_, this)) {
  model_observer_.Add(model_);
  AddMainButton(extensions_button_);
  CreateActions();
}

ExtensionsToolbarContainer::~ExtensionsToolbarContainer() {
  if (active_bubble_)
    active_bubble_->GetWidget()->Close();
  // We should synchronously receive the OnWidgetClosing() event, so we should
  // always have cleared the active bubble by now.
  DCHECK(!active_bubble_);
}

void ExtensionsToolbarContainer::UpdateAllIcons() {
  extensions_button_->UpdateIcon();

  for (const auto& action : actions_)
    action->UpdateState();
}

ToolbarActionView* ExtensionsToolbarContainer::GetViewForId(
    const std::string& id) {
  auto it = icons_.find(id);
  if (it == icons_.end())
    return nullptr;
  return it->second.get();
}

ToolbarActionViewController* ExtensionsToolbarContainer::GetActionForId(
    const std::string& action_id) {
  for (const auto& action : actions_) {
    if (action->GetId() == action_id)
      return action.get();
  }
  return nullptr;
}

ToolbarActionViewController* ExtensionsToolbarContainer::GetPoppedOutAction()
    const {
  return popped_out_action_;
}

bool ExtensionsToolbarContainer::IsActionVisibleOnToolbar(
    const ToolbarActionViewController* action) const {
  return model_->IsActionPinned(action->GetId()) ||
         action == popped_out_action_ ||
         (active_bubble_ &&
          action->GetId() == active_bubble_->GetAnchorActionId());
}

void ExtensionsToolbarContainer::UndoPopOut() {
  DCHECK(popped_out_action_);
  ToolbarActionViewController* const popped_out_action = popped_out_action_;
  popped_out_action_ = nullptr;
  // Note that we only hide this view if it was not pinned while being popped
  // out.
  icons_[popped_out_action->GetId()]->SetVisible(
      IsActionVisibleOnToolbar(popped_out_action));
}

void ExtensionsToolbarContainer::SetPopupOwner(
    ToolbarActionViewController* popup_owner) {
  // We should never be setting a popup owner when one already exists, and
  // never unsetting one when one wasn't set.
  DCHECK((popup_owner_ != nullptr) ^ (popup_owner != nullptr));
  popup_owner_ = popup_owner;
}

void ExtensionsToolbarContainer::HideActivePopup() {
  if (popup_owner_)
    popup_owner_->HidePopup();
  DCHECK(!popup_owner_);
}

bool ExtensionsToolbarContainer::CloseOverflowMenuIfOpen() {
  if (ExtensionsMenuView::IsShowing()) {
    ExtensionsMenuView::Hide();
    return true;
  }
  return false;
}

void ExtensionsToolbarContainer::PopOutAction(
    ToolbarActionViewController* action,
    bool is_sticky,
    const base::Closure& closure) {
  // TODO(pbos): Animate popout.
  // TODO(pbos): Highlight popout differently.
  DCHECK(!popped_out_action_);
  popped_out_action_ = action;
  icons_[popped_out_action_->GetId()]->SetVisible(true);
  ReorderViews();
  closure.Run();
}

void ExtensionsToolbarContainer::ShowToolbarActionBubble(
    std::unique_ptr<ToolbarActionsBarBubbleDelegate> controller) {
  // TODO(pbos): Make sure we finish animations before showing the bubble.

  auto iter = icons_.find(controller->GetAnchorActionId());

  views::View* const anchor_view = iter != icons_.end()
                                       ? static_cast<View*>(iter->second.get())
                                       : extensions_button_;

  anchor_view->SetVisible(true);

  active_bubble_ = new ToolbarActionsBarBubbleViews(
      anchor_view, gfx::Point(), anchor_view != extensions_button_,
      std::move(controller));
  views::BubbleDialogDelegateView::CreateBubble(active_bubble_)
      ->AddObserver(this);
  active_bubble_->Show();
}

void ExtensionsToolbarContainer::ShowToolbarActionBubbleAsync(
    std::unique_ptr<ToolbarActionsBarBubbleDelegate> bubble) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&ExtensionsToolbarContainer::ShowToolbarActionBubble,
                     weak_ptr_factory_.GetWeakPtr(), std::move(bubble)));
}

void ExtensionsToolbarContainer::OnToolbarActionAdded(
    const ToolbarActionsModel::ActionId& action_id,
    int index) {
  CreateActionForId(action_id);
  ReorderViews();
}

void ExtensionsToolbarContainer::OnToolbarActionRemoved(
    const ToolbarActionsModel::ActionId& action_id) {
  // TODO(pbos): Handle extension upgrades, see ToolbarActionsBar. Arguably this
  // could be handled inside the model and be invisible to the container when
  // permissions are unchanged.

  // Delete the icon first so it unregisters it from the action.
  icons_.erase(action_id);
  base::EraseIf(
      actions_,
      [action_id](const std::unique_ptr<ToolbarActionViewController>& item) {
        return item->GetId() == action_id;
      });
}

void ExtensionsToolbarContainer::OnToolbarActionMoved(
    const ToolbarActionsModel::ActionId& action_id,
    int index) {}

void ExtensionsToolbarContainer::OnToolbarActionLoadFailed() {}

void ExtensionsToolbarContainer::OnToolbarActionUpdated(
    const ToolbarActionsModel::ActionId& action_id) {
  ToolbarActionViewController* action = GetActionForId(action_id);
  if (action)
    action->UpdateState();
}

void ExtensionsToolbarContainer::OnToolbarVisibleCountChanged() {}

void ExtensionsToolbarContainer::OnToolbarHighlightModeChanged(
    bool is_highlighting) {}

void ExtensionsToolbarContainer::OnToolbarModelInitialized() {
  CreateActions();
}

void ExtensionsToolbarContainer::OnToolbarPinnedActionsChanged() {
  for (auto& it : icons_)
    it.second->SetVisible(IsActionVisibleOnToolbar(GetActionForId(it.first)));
  ReorderViews();
}

void ExtensionsToolbarContainer::ReorderViews() {
  // TODO(pbos): Reorder pinned actions here once they exist.
  const auto& pinned_action_ids = model_->pinned_action_ids();
  for (size_t i = 0; i < pinned_action_ids.size(); ++i)
    ReorderChildView(icons_[pinned_action_ids[i]].get(), i);

  // Popped out actions should be at the end.
  if (popped_out_action_)
    ReorderChildView(icons_[popped_out_action_->GetId()].get(), -1);

  // The extension button is always last.
  ReorderChildView(extensions_button_, -1);
}

void ExtensionsToolbarContainer::CreateActions() {
  DCHECK(icons_.empty());
  DCHECK(actions_.empty());

  // If the model isn't initialized, wait for it.
  if (!model_->actions_initialized())
    return;

  for (auto& action_id : model_->action_ids())
    CreateActionForId(action_id);

  ReorderViews();
}

void ExtensionsToolbarContainer::CreateActionForId(
    const ToolbarActionsModel::ActionId& action_id) {
  actions_.push_back(
      model_->CreateActionForId(browser_, this, false, action_id));

  auto icon = std::make_unique<ToolbarActionView>(actions_.back().get(), this);
  icon->set_owned_by_client();
  icon->SetVisible(IsActionVisibleOnToolbar(actions_.back().get()));
  icon->AddButtonObserver(this);
  icon->AddObserver(this);
  AddChildView(icon.get());

  icons_[action_id] = std::move(icon);
}

content::WebContents* ExtensionsToolbarContainer::GetCurrentWebContents() {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

bool ExtensionsToolbarContainer::ShownInsideMenu() const {
  return false;
}

void ExtensionsToolbarContainer::OnToolbarActionViewDragDone() {}

views::LabelButton* ExtensionsToolbarContainer::GetOverflowReferenceView() {
  return extensions_button_;
}

gfx::Size ExtensionsToolbarContainer::GetToolbarActionSize() {
  gfx::Rect rect(gfx::Size(28, 28));
  rect.Inset(-GetLayoutInsets(TOOLBAR_ACTION_VIEW));
  return rect.size();
}

void ExtensionsToolbarContainer::WriteDragDataForView(
    View* sender,
    const gfx::Point& press_pt,
    ui::OSExchangeData* data) {}

int ExtensionsToolbarContainer::GetDragOperationsForView(View* sender,
                                                         const gfx::Point& p) {
  return ui::DragDropTypes::DRAG_MOVE;
}

bool ExtensionsToolbarContainer::CanStartDragForView(View* sender,
                                                     const gfx::Point& press_pt,
                                                     const gfx::Point& p) {
  // TODO(pbos): Implement
  return false;
}

void ExtensionsToolbarContainer::OnWidgetClosing(views::Widget* widget) {
  ClearActiveBubble(widget);
}

void ExtensionsToolbarContainer::OnWidgetDestroying(views::Widget* widget) {
  ClearActiveBubble(widget);
}

void ExtensionsToolbarContainer::ClearActiveBubble(views::Widget* widget) {
  DCHECK(active_bubble_);
  DCHECK_EQ(active_bubble_->GetWidget(), widget);
  ToolbarActionViewController* const action =
      GetActionForId(active_bubble_->GetAnchorActionId());
  // TODO(pbos): Note that this crashes if a bubble anchors to the menu and not
  // to an extension that gets popped out. This should be fixed, but a test
  // should first be added to make sure that it's covered.
  CHECK(action);
  active_bubble_ = nullptr;
  widget->RemoveObserver(this);
  // Note that we only hide this view if it's not visible for other reasons
  // than displaying the bubble.
  icons_[action->GetId()]->SetVisible(IsActionVisibleOnToolbar(action));
}
