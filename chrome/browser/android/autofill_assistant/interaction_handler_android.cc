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
#include "components/autofill_assistant/browser/basic_interactions.h"
#include "components/autofill_assistant/browser/generic_ui.pb.h"
#include "components/autofill_assistant/browser/ui_delegate.h"
#include "components/autofill_assistant/browser/user_model.h"
#include "components/autofill_assistant/browser/value_util.h"

namespace autofill_assistant {

namespace {

base::Optional<EventHandler::EventKey> CreateEventKeyFromProto(
    const EventProto& proto,
    std::map<std::string, base::android::ScopedJavaGlobalRef<jobject>>* views,
    base::android::ScopedJavaGlobalRef<jobject> jdelegate) {
  switch (proto.kind_case()) {
    case EventProto::kOnValueChanged:
      if (proto.on_value_changed().model_identifier().empty()) {
        VLOG(1) << "Invalid OnValueChangedEventProto: no model_identifier "
                   "specified";
        return base::nullopt;
      }
      return base::Optional<EventHandler::EventKey>(
          {proto.kind_case(), proto.on_value_changed().model_identifier()});
    case EventProto::kOnViewClicked:
      if (proto.on_view_clicked().view_identifier().empty()) {
        VLOG(1) << "Invalid OnViewClickedEventProto: no view_identifier "
                   "specified";
        return base::nullopt;
      }
      return base::Optional<EventHandler::EventKey>(
          {proto.kind_case(), proto.on_view_clicked().view_identifier()});
    case EventProto::kOnUserActionCalled:
      if (proto.on_user_action_called().user_action_identifier().empty()) {
        VLOG(1) << "Invalid OnUserActionCalled: no user_action_identifier "
                   "specified";
        return base::nullopt;
      }
      return base::Optional<EventHandler::EventKey>(
          {proto.kind_case(),
           proto.on_user_action_called().user_action_identifier()});
    case EventProto::kOnTextLinkClicked:
      if (!proto.on_text_link_clicked().has_text_link()) {
        VLOG(1) << "Invalid OnTextLinkClickedProto: no text_link specified";
        return base::nullopt;
      }
      return base::Optional<EventHandler::EventKey>(
          {proto.kind_case(),
           base::NumberToString(proto.on_text_link_clicked().text_link())});
    case EventProto::kOnPopupDismissed:
      if (proto.on_popup_dismissed().popup_identifier().empty()) {
        VLOG(1)
            << "Invalid OnPopupDismissedProto: no popup_identifier specified";
        return base::nullopt;
      }
      return base::Optional<EventHandler::EventKey>(
          {proto.kind_case(), proto.on_popup_dismissed().popup_identifier()});
    case EventProto::KIND_NOT_SET:
      VLOG(1) << "Error creating event: kind not set";
      return base::nullopt;
  }
}

}  // namespace

InteractionHandlerAndroid::InteractionHandlerAndroid(
    EventHandler* event_handler,
    UserModel* user_model,
    BasicInteractions* basic_interactions,
    std::map<std::string, base::android::ScopedJavaGlobalRef<jobject>>* views,
    base::android::ScopedJavaGlobalRef<jobject> jcontext,
    base::android::ScopedJavaGlobalRef<jobject> jdelegate)
    : event_handler_(event_handler),
      user_model_(user_model),
      basic_interactions_(basic_interactions),
      views_(views),
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
  auto key = CreateEventKeyFromProto(proto.trigger_event(), views_, jdelegate_);
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
          /* view_inflation_successful = */ true, proto.end_action()));
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
          proto.set_text(), views_, jdelegate_));
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
          proto.set_view_visibility(), views_));
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
          proto.set_view_enabled(), views_));
    case CallbackProto::kShowGenericPopup:
      if (proto.show_generic_popup().popup_identifier().empty()) {
        VLOG(1) << "Error creating ShowGenericPopup interaction: "
                   "popup_identifier not set";
        return base::nullopt;
      }
      return base::Optional<InteractionCallback>(base::BindRepeating(
          &InteractionHandlerAndroid::CreateAndShowGenericPopup, GetWeakPtr(),
          proto.show_generic_popup()));
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
  auto nested_ui = GenericUiControllerAndroid::CreateFromProto(
      proto, jcontext_, jdelegate_, event_handler_, user_model_,
      basic_interactions_);
  const auto* nested_ui_ptr = nested_ui.get();
  if (nested_ui) {
    DCHECK(nested_ui_controllers_.find(identifier) ==
           nested_ui_controllers_.end());
    nested_ui_controllers_.emplace(identifier, std::move(nested_ui));
  }
  return nested_ui_ptr;
}

void InteractionHandlerAndroid::CreateAndShowGenericPopup(
    const ShowGenericUiPopupProto& proto) {
  auto* nested_ui =
      CreateNestedUi(proto.generic_ui(), proto.popup_identifier());
  if (!nested_ui) {
    DVLOG(2) << "Error showing popup: error creating generic UI";
    return;
  }
  AddInteraction({EventProto::kOnPopupDismissed, proto.popup_identifier()},
                 base::BindRepeating(&InteractionHandlerAndroid::DeleteNestedUi,
                                     GetWeakPtr(), proto.popup_identifier()));
  android_interactions::ShowGenericPopup(proto, nested_ui->GetRootView(),
                                         jcontext_, jdelegate_);
}

}  // namespace autofill_assistant
