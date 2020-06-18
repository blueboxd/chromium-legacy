// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/autofill_assistant/interaction_handler_android.h"
#include <algorithm>
#include <vector>
#include "base/android/jni_string.h"
#include "base/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/android/autofill_assistant/generic_ui_controller_android.h"
#include "chrome/browser/android/autofill_assistant/generic_ui_interactions_android.h"
#include "chrome/browser/android/autofill_assistant/view_handler_android.h"
#include "components/autofill_assistant/browser/basic_interactions.h"
#include "components/autofill_assistant/browser/generic_ui.pb.h"
#include "components/autofill_assistant/browser/ui_delegate.h"
#include "components/autofill_assistant/browser/user_model.h"
#include "components/autofill_assistant/browser/value_util.h"

namespace autofill_assistant {

InteractionHandlerAndroid::InteractionHandlerAndroid(
    EventHandler* event_handler,
    UserModel* user_model,
    BasicInteractions* basic_interactions,
    ViewHandlerAndroid* view_handler,
    base::android::ScopedJavaGlobalRef<jobject> jcontext,
    base::android::ScopedJavaGlobalRef<jobject> jdelegate)
    : event_handler_(event_handler),
      user_model_(user_model),
      basic_interactions_(basic_interactions),
      view_handler_(view_handler),
      jcontext_(jcontext),
      jdelegate_(jdelegate) {}

InteractionHandlerAndroid::~InteractionHandlerAndroid() {
  event_handler_->RemoveObserver(this);
}

base::WeakPtr<InteractionHandlerAndroid>
InteractionHandlerAndroid::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void InteractionHandlerAndroid::StartListening() {
  is_listening_ = true;
  event_handler_->AddObserver(this);
}

void InteractionHandlerAndroid::StopListening() {
  event_handler_->RemoveObserver(this);
  is_listening_ = false;
}

UserModel* InteractionHandlerAndroid::GetUserModel() const {
  return user_model_;
}

BasicInteractions* InteractionHandlerAndroid::GetBasicInteractions() const {
  return basic_interactions_;
}

bool InteractionHandlerAndroid::AddInteractionsFromProto(
    const InteractionProto& proto) {
  if (is_listening_) {
    NOTREACHED() << "Interactions can not be added while listening to events!";
    return false;
  }
  auto key = EventHandler::CreateEventKeyFromProto(proto.trigger_event());
  if (!key) {
    VLOG(1) << "Invalid trigger event for interaction";
    return false;
  }

  for (const auto& callback_proto : proto.callbacks()) {
    auto callback = CreateInteractionCallbackFromProto(callback_proto);
    if (!callback) {
      VLOG(1) << "Invalid callback for interaction";
      return false;
    }
    // Wrap callback in condition handler if necessary.
    if (callback_proto.has_condition_model_identifier()) {
      callback = base::Optional<InteractionCallback>(base::BindRepeating(
          &android_interactions::RunConditionalCallback,
          basic_interactions_->GetWeakPtr(),
          callback_proto.condition_model_identifier(), *callback));
    }
    AddInteraction(*key, *callback);
  }
  return true;
}

void InteractionHandlerAndroid::AddInteraction(
    const EventHandler::EventKey& key,
    const InteractionCallback& callback) {
  interactions_[key].emplace_back(callback);
}

void InteractionHandlerAndroid::OnEvent(const EventHandler::EventKey& key) {
  auto it = interactions_.find(key);
  if (it != interactions_.end()) {
    for (auto& callback : it->second) {
      callback.Run();
    }
  }
}

void InteractionHandlerAndroid::AddRadioButtonToGroup(
    const std::string& radio_group,
    const std::string& model_identifier) {
  radio_groups_[radio_group].emplace_back(model_identifier);
}

void InteractionHandlerAndroid::UpdateRadioButtonGroup(
    const std::string& radio_group,
    const std::string& selected_model_identifier) {
  if (radio_groups_.find(radio_group) == radio_groups_.end()) {
    return;
  }
  basic_interactions_->UpdateRadioButtonGroup(radio_groups_[radio_group],
                                              selected_model_identifier);
}

base::Optional<InteractionHandlerAndroid::InteractionCallback>
InteractionHandlerAndroid::CreateInteractionCallbackFromProto(
    const CallbackProto& proto) {
  switch (proto.kind_case()) {
    case CallbackProto::kSetValue:
      if (!proto.set_value().has_value()) {
        VLOG(1) << "Error creating SetValue interaction: value "
                   "not set";
        return base::nullopt;
      }
      return base::Optional<InteractionCallback>(base::BindRepeating(
          &android_interactions::SetValue, basic_interactions_->GetWeakPtr(),
          proto.set_value()));
    case CallbackProto::kShowInfoPopup: {
      return base::Optional<InteractionCallback>(
          base::BindRepeating(&android_interactions::ShowInfoPopup,
                              proto.show_info_popup().info_popup(), jcontext_));
    }
    case CallbackProto::kShowListPopup:
      if (!proto.show_list_popup().has_item_names()) {
        VLOG(1) << "Error creating ShowListPopup interaction: "
                   "item_names not set";
        return base::nullopt;
      }
      if (proto.show_list_popup()
              .selected_item_indices_model_identifier()
              .empty()) {
        VLOG(1) << "Error creating ShowListPopup interaction: "
                   "selected_item_indices_model_identifier not set";
        return base::nullopt;
      }
      return base::Optional<InteractionCallback>(base::BindRepeating(
          &android_interactions::ShowListPopup, user_model_->GetWeakPtr(),
          proto.show_list_popup(), jcontext_, jdelegate_));
    case CallbackProto::kComputeValue:
      if (proto.compute_value().result_model_identifier().empty()) {
        VLOG(1) << "Error creating ComputeValue interaction: "
                   "result_model_identifier empty";
        return base::nullopt;
      }
      return base::Optional<InteractionCallback>(base::BindRepeating(
          &android_interactions::ComputeValue,
          basic_interactions_->GetWeakPtr(), proto.compute_value()));
    case CallbackProto::kSetUserActions:
      if (!proto.set_user_actions().has_user_actions()) {
        VLOG(1) << "Error creating SetUserActions interaction: "
                   "user_actions not set";
        return base::nullopt;
      }
      return base::Optional<InteractionCallback>(base::BindRepeating(
          &android_interactions::SetUserActions,
          basic_interactions_->GetWeakPtr(), proto.set_user_actions()));
    case CallbackProto::kEndAction:
      return base::Optional<InteractionCallback>(base::BindRepeating(
          &android_interactions::EndAction, basic_interactions_->GetWeakPtr(),
          proto.end_action()));
    case CallbackProto::kShowCalendarPopup:
      if (proto.show_calendar_popup().date_model_identifier().empty()) {
        VLOG(1) << "Error creating ShowCalendarPopup interaction: "
                   "date_model_identifier not set";
        return base::nullopt;
      }
      return base::Optional<InteractionCallback>(base::BindRepeating(
          &android_interactions::ShowCalendarPopup, user_model_->GetWeakPtr(),
          proto.show_calendar_popup(), jcontext_, jdelegate_));
    case CallbackProto::kSetText:
      if (!proto.set_text().has_text()) {
        VLOG(1) << "Error creating SetText interaction: "
                   "text not set";
        return base::nullopt;
      }
      if (proto.set_text().view_identifier().empty()) {
        VLOG(1) << "Error creating SetText interaction: "
                   "view_identifier not set";
        return base::nullopt;
      }
      return base::Optional<InteractionCallback>(base::BindRepeating(
          &android_interactions::SetViewText, user_model_->GetWeakPtr(),
          proto.set_text(), view_handler_, jdelegate_));
    case CallbackProto::kToggleUserAction:
      if (proto.toggle_user_action().user_actions_model_identifier().empty()) {
        VLOG(1) << "Error creating ToggleUserAction interaction: "
                   "user_actions_model_identifier not set";
        return base::nullopt;
      }
      if (proto.toggle_user_action().user_action_identifier().empty()) {
        VLOG(1) << "Error creating ToggleUserAction interaction: "
                   "user_action_identifier not set";
        return base::nullopt;
      }
      if (!proto.toggle_user_action().has_enabled()) {
        VLOG(1) << "Error creating ToggleUserAction interaction: "
                   "enabled not set";
        return base::nullopt;
      }
      return base::Optional<InteractionCallback>(base::BindRepeating(
          &android_interactions::ToggleUserAction,
          basic_interactions_->GetWeakPtr(), proto.toggle_user_action()));
    case CallbackProto::kSetViewVisibility:
      if (proto.set_view_visibility().view_identifier().empty()) {
        VLOG(1) << "Error creating SetViewVisibility interaction: "
                   "view_identifier not set";
        return base::nullopt;
      }
      if (!proto.set_view_visibility().has_visible()) {
        VLOG(1) << "Error creating SetViewVisibility interaction: "
                   "visible not set";
        return base::nullopt;
      }
      return base::Optional<InteractionCallback>(base::BindRepeating(
          &android_interactions::SetViewVisibility, user_model_->GetWeakPtr(),
          proto.set_view_visibility(), view_handler_));
    case CallbackProto::kSetViewEnabled:
      if (proto.set_view_enabled().view_identifier().empty()) {
        VLOG(1) << "Error creating SetViewEnabled interaction: "
                   "view_identifier not set";
        return base::nullopt;
      }
      if (!proto.set_view_enabled().has_enabled()) {
        VLOG(1) << "Error creating SetViewEnabled interaction: "
                   "enabled not set";
        return base::nullopt;
      }
      return base::Optional<InteractionCallback>(base::BindRepeating(
          &android_interactions::SetViewEnabled, user_model_->GetWeakPtr(),
          proto.set_view_enabled(), view_handler_));
    case CallbackProto::kShowGenericPopup:
      if (proto.show_generic_popup().popup_identifier().empty()) {
        VLOG(1) << "Error creating ShowGenericPopup interaction: "
                   "popup_identifier not set";
        return base::nullopt;
      }
      return base::Optional<InteractionCallback>(base::BindRepeating(
          &InteractionHandlerAndroid::CreateAndShowGenericPopup, GetWeakPtr(),
          proto.show_generic_popup()));
    case CallbackProto::kCreateNestedUi:
      if (proto.create_nested_ui().generic_ui_identifier().empty()) {
        VLOG(1) << "Error creating CreateNestedGenericUi interaction: "
                   "generic_ui_identifier not set";
        return base::nullopt;
      }
      return base::Optional<InteractionCallback>(base::BindRepeating(
          &InteractionHandlerAndroid::CreateAndAttachNestedGenericUi,
          GetWeakPtr(), proto.create_nested_ui()));
    case CallbackProto::kClearViewContainer:
      if (proto.clear_view_container().view_identifier().empty()) {
        VLOG(1) << "Error creating ClearViewContainer interaction: "
                   "view_identifier not set";
        return base::nullopt;
      }
      return base::Optional<InteractionCallback>(
          base::BindRepeating(&android_interactions::ClearViewContainer,
                              proto.clear_view_container().view_identifier(),
                              view_handler_, jdelegate_));
    case CallbackProto::KIND_NOT_SET:
      VLOG(1) << "Error creating interaction: kind not set";
      return base::nullopt;
  }
}

void InteractionHandlerAndroid::DeleteNestedUi(const std::string& identifier) {
  auto it = nested_ui_controllers_.find(identifier);
  if (it != nested_ui_controllers_.end()) {
    nested_ui_controllers_.erase(it);
  }
}

const GenericUiControllerAndroid* InteractionHandlerAndroid::CreateNestedUi(
    const GenericUserInterfaceProto& proto,
    const std::string& identifier) {
  if (nested_ui_controllers_.find(identifier) != nested_ui_controllers_.end()) {
    VLOG(2) << "Error creating nested UI: " << identifier
            << " already exixsts (did you forget to clear the previous "
               "instance with ClearViewContainerProto?)";
    return nullptr;
  }
  auto nested_ui = GenericUiControllerAndroid::CreateFromProto(
      proto, jcontext_, jdelegate_, event_handler_, user_model_,
      basic_interactions_);
  const auto* nested_ui_ptr = nested_ui.get();
  if (nested_ui) {
    nested_ui_controllers_.emplace(identifier, std::move(nested_ui));
  } else {
    VLOG(2) << "Error creating nested UI " << identifier
            << ": view inflation failed";
  }
  return nested_ui_ptr;
}

void InteractionHandlerAndroid::CreateAndAttachNestedGenericUi(
    const CreateNestedGenericUiProto& proto) {
  auto* nested_ui =
      CreateNestedUi(proto.generic_ui(), proto.generic_ui_identifier());
  if (!nested_ui) {
    return;
  }

  if (!android_interactions::AttachViewToParent(nested_ui->GetRootView(),
                                                proto.parent_view_identifier(),
                                                view_handler_)) {
    DeleteNestedUi(proto.generic_ui_identifier());
    return;
  }

  AddInteraction(
      {EventProto::kOnViewContainerCleared, proto.parent_view_identifier()},
      base::BindRepeating(&InteractionHandlerAndroid::DeleteNestedUi,
                          GetWeakPtr(), proto.generic_ui_identifier()));
}

void InteractionHandlerAndroid::CreateAndShowGenericPopup(
    const ShowGenericUiPopupProto& proto) {
  auto* nested_ui =
      CreateNestedUi(proto.generic_ui(), proto.popup_identifier());
  if (!nested_ui) {
    return;
  }
  AddInteraction({EventProto::kOnPopupDismissed, proto.popup_identifier()},
                 base::BindRepeating(&InteractionHandlerAndroid::DeleteNestedUi,
                                     GetWeakPtr(), proto.popup_identifier()));
  android_interactions::ShowGenericPopup(proto, nested_ui->GetRootView(),
                                         jcontext_, jdelegate_);
}

void InteractionHandlerAndroid::RunValueChangedCallbacks() {
  for (const auto& interaction : interactions_) {
    if (interaction.first.first == EventProto::kOnValueChanged) {
      OnEvent(interaction.first);
    }
  }
}

}  // namespace autofill_assistant
