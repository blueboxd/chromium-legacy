// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/autofill_assistant/interaction_handler_android.h"
#include <algorithm>
#include <vector>
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/android/features/autofill_assistant/jni_headers/AssistantViewInteractions_jni.h"
#include "chrome/browser/android/autofill_assistant/generic_ui_controller_android.h"
#include "chrome/browser/android/autofill_assistant/ui_controller_android_utils.h"
#include "components/autofill_assistant/browser/basic_interactions.h"
#include "components/autofill_assistant/browser/interactions.pb.h"
#include "components/autofill_assistant/browser/ui_delegate.h"
#include "components/autofill_assistant/browser/user_model.h"

namespace autofill_assistant {

namespace {

// Note regarding Try* methods: these are simple wrappers around basic
// interactions. They are needed because it is impossible to directly bind to
// non-void return methods with a weak ptr.
void TrySetValue(base::WeakPtr<BasicInteractions> basic_interactions,
                 const SetModelValueProto& proto) {
  if (!basic_interactions) {
    return;
  }
  basic_interactions->SetValue(proto);
}

void TryComputeValue(base::WeakPtr<BasicInteractions> basic_interactions,
                     const ComputeValueProto& proto) {
  if (!basic_interactions) {
    return;
  }
  basic_interactions->ComputeValue(proto);
}

void SetUserActionsIgnoreReturn(
    base::WeakPtr<BasicInteractions> basic_interactions,
    const SetUserActionsProto& proto) {
  if (!basic_interactions) {
    return;
  }
  basic_interactions->SetUserActions(proto);
}

void TryEndAction(base::WeakPtr<BasicInteractions> basic_interactions,
                  const EndActionProto& proto) {
  if (!basic_interactions) {
    return;
  }
  basic_interactions->EndAction(proto);
}

void TryToggleUserAction(base::WeakPtr<BasicInteractions> basic_interactions,
                         const ToggleUserActionProto& proto) {
  if (!basic_interactions) {
    return;
  }
  basic_interactions->ToggleUserAction(proto);
}

void TryRunConditionalCallback(
    base::WeakPtr<BasicInteractions> basic_interactions,
    const std::string& condition_identifier,
    InteractionHandlerAndroid::InteractionCallback callback) {
  if (!basic_interactions) {
    return;
  }
  basic_interactions->RunConditionalCallback(condition_identifier, callback);
}

void ShowInfoPopup(const InfoPopupProto& proto,
                   base::android::ScopedJavaGlobalRef<jobject> jcontext) {
  JNIEnv* env = base::android::AttachCurrentThread();
  auto jcontext_local = base::android::ScopedJavaLocalRef<jobject>(jcontext);
  ui_controller_android_utils::ShowJavaInfoPopup(
      env, ui_controller_android_utils::CreateJavaInfoPopup(env, proto),
      jcontext_local);
}

void ShowListPopup(base::WeakPtr<UserModel> user_model,
                   const ShowListPopupProto& proto,
                   base::android::ScopedJavaGlobalRef<jobject> jcontext,
                   base::android::ScopedJavaGlobalRef<jobject> jdelegate) {
  if (!user_model) {
    return;
  }

  auto item_names = user_model->GetValue(proto.item_names_model_identifier());
  if (!item_names.has_value()) {
    DVLOG(2) << "Failed to show list popup: '"
             << proto.item_names_model_identifier() << "' not found in model.";
    return;
  }
  if (item_names->strings().values().size() == 0) {
    DVLOG(2) << "Failed to show list popup: the list of item names in '"
             << proto.item_names_model_identifier() << "' was empty.";
    return;
  }

  base::Optional<ValueProto> item_types;
  if (proto.has_item_types_model_identifier()) {
    item_types = user_model->GetValue(proto.item_types_model_identifier());
    if (!item_types.has_value()) {
      DVLOG(2) << "Failed to show list popup: '"
               << proto.item_types_model_identifier()
               << "' not found in the model.";
      return;
    }
    if (item_types->ints().values().size() !=
        item_names->strings().values().size()) {
      DVLOG(2) << "Failed to show list popup: Expected item_types to contain "
               << item_names->strings().values().size() << " integers, but got "
               << item_types->ints().values().size();
      return;
    }
  } else {
    item_types = ValueProto();
    for (int i = 0; i < item_names->strings().values().size(); ++i) {
      item_types->mutable_ints()->add_values(
          static_cast<int>(ShowListPopupProto::ENABLED));
    }
  }

  auto selected_indices =
      user_model->GetValue(proto.selected_item_indices_model_identifier());
  if (!selected_indices.has_value()) {
    DVLOG(2) << "Failed to show list popup: '"
             << proto.selected_item_indices_model_identifier()
             << "' not found in model.";
    return;
  }
  if (!(*selected_indices == ValueProto()) &&
      selected_indices->kind_case() != ValueProto::kInts) {
    DVLOG(2) << "Failed to show list popup: expected '"
             << proto.selected_item_indices_model_identifier()
             << "' to be int[], but was of type "
             << selected_indices->kind_case();
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  std::vector<std::string> item_names_vec;
  std::copy(item_names->strings().values().begin(),
            item_names->strings().values().end(),
            std::back_inserter(item_names_vec));

  std::vector<int> item_types_vec;
  std::copy(item_types->ints().values().begin(),
            item_types->ints().values().end(),
            std::back_inserter(item_types_vec));

  std::vector<int> selected_indices_vec;
  std::copy(selected_indices->ints().values().begin(),
            selected_indices->ints().values().end(),
            std::back_inserter(selected_indices_vec));

  Java_AssistantViewInteractions_showListPopup(
      env, jcontext, base::android::ToJavaArrayOfStrings(env, item_names_vec),
      base::android::ToJavaIntArray(env, item_types_vec),
      base::android::ToJavaIntArray(env, selected_indices_vec),
      proto.allow_multiselect(),
      base::android::ConvertUTF8ToJavaString(
          env, proto.selected_item_indices_model_identifier()),
      proto.selected_item_names_model_identifier().empty()
          ? nullptr
          : base::android::ConvertUTF8ToJavaString(
                env, proto.selected_item_names_model_identifier()),
      jdelegate);
}

void ShowCalendarPopup(base::WeakPtr<UserModel> user_model,
                       const ShowCalendarPopupProto& proto,
                       base::android::ScopedJavaGlobalRef<jobject> jcontext,
                       base::android::ScopedJavaGlobalRef<jobject> jdelegate) {
  if (!user_model) {
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  auto initial_date = user_model->GetValue(proto.date_model_identifier());
  if (initial_date.has_value() && initial_date->dates().values_size() != 1) {
    DVLOG(2) << "Failed to show calendar popup: date_model_identifier must be "
                "empty or contain single date, but was "
             << *initial_date;
    return;
  }

  auto min_date = user_model->GetValue(proto.min_date_model_identifier());
  if (!min_date.has_value() || min_date->dates().values_size() != 1) {
    DVLOG(2) << "Failed to show calendar popup: min_date not found or invalid "
                "in user model at "
             << proto.min_date_model_identifier();
    return;
  }

  auto max_date = user_model->GetValue(proto.max_date_model_identifier());
  if (!max_date.has_value() || max_date->dates().values_size() != 1) {
    DVLOG(2) << "Failed to show calendar popup: max_date not found or invalid "
                "in user model at "
             << proto.max_date_model_identifier();
    return;
  }

  jboolean jsuccess = Java_AssistantViewInteractions_showCalendarPopup(
      env, jcontext,
      initial_date.has_value()
          ? ui_controller_android_utils::ToJavaValue(env, *initial_date)
          : nullptr,
      ui_controller_android_utils::ToJavaValue(env, *min_date),
      ui_controller_android_utils::ToJavaValue(env, *max_date),
      base::android::ConvertUTF8ToJavaString(env,
                                             proto.date_model_identifier()),
      jdelegate);
  if (!jsuccess) {
    DVLOG(2) << "Failed to show calendar popup: JNI call failed";
  }
}

void SetTextViewText(
    base::WeakPtr<UserModel> user_model,
    const SetTextProto& proto,
    std::map<std::string, base::android::ScopedJavaGlobalRef<jobject>>* views) {
  if (!user_model) {
    return;
  }

  auto text = user_model->GetValue(proto.model_identifier());
  if (!text.has_value()) {
    DVLOG(2) << "Failed to set text for " << proto.view_identifier() << ": "
             << proto.model_identifier() << " not found in model";
    return;
  }
  if (text->strings().values_size() != 1) {
    DVLOG(2) << "Failed to set text for " << proto.view_identifier()
             << ": expected " << proto.model_identifier()
             << " to contain single string, but was instead " << *text;
    return;
  }

  auto jview = views->find(proto.view_identifier());
  if (jview == views->end()) {
    DVLOG(2) << "Failed to set text for " << proto.view_identifier() << ": "
             << " view not found";
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AssistantViewInteractions_setTextViewText(
      env, jview->second,
      base::android::ConvertUTF8ToJavaString(env, text->strings().values(0)));
}

base::Optional<EventHandler::EventKey> CreateEventKeyFromProto(
    const EventProto& proto,
    JNIEnv* env,
    std::map<std::string, base::android::ScopedJavaGlobalRef<jobject>>* views,
    base::android::ScopedJavaGlobalRef<jobject> jdelegate) {
  switch (proto.kind_case()) {
    case EventProto::kOnValueChanged:
      return base::Optional<EventHandler::EventKey>(
          {proto.kind_case(), proto.on_value_changed().model_identifier()});
    case EventProto::kOnViewClicked: {
      auto jview = views->find(proto.on_view_clicked().view_identifier());
      if (jview == views->end()) {
        VLOG(1) << "Invalid click event, no view with id='"
                << proto.on_view_clicked().view_identifier() << "' found";
        return base::nullopt;
      }
      Java_AssistantViewInteractions_setOnClickListener(
          env, jview->second,
          base::android::ConvertUTF8ToJavaString(
              env, proto.on_view_clicked().view_identifier()),
          jdelegate);
      return base::Optional<EventHandler::EventKey>(
          {proto.kind_case(), proto.on_view_clicked().view_identifier()});
    }
    case EventProto::kOnUserActionCalled: {
      return base::Optional<EventHandler::EventKey>(
          {proto.kind_case(),
           proto.on_user_action_called().user_action_identifier()});
    }
    case EventProto::KIND_NOT_SET:
      VLOG(1) << "Error creating event: kind not set";
      return base::nullopt;
  }
}

base::Optional<InteractionHandlerAndroid::InteractionCallback>
CreateInteractionCallbackFromProto(
    const CallbackProto& proto,
    UserModel* user_model,
    BasicInteractions* basic_interactions,
    std::map<std::string, base::android::ScopedJavaGlobalRef<jobject>>* views,
    base::android::ScopedJavaGlobalRef<jobject> jcontext,
    base::android::ScopedJavaGlobalRef<jobject> jdelegate) {
  switch (proto.kind_case()) {
    case CallbackProto::kSetValue:
      if (proto.set_value().model_identifier().empty()) {
        VLOG(1)
            << "Error creating SetValue interaction: model_identifier not set";
        return base::nullopt;
      }
      return base::Optional<InteractionHandlerAndroid::InteractionCallback>(
          base::BindRepeating(&TrySetValue, basic_interactions->GetWeakPtr(),
                              proto.set_value()));
    case CallbackProto::kShowInfoPopup: {
      return base::Optional<InteractionHandlerAndroid::InteractionCallback>(
          base::BindRepeating(&ShowInfoPopup,
                              proto.show_info_popup().info_popup(), jcontext));
    }
    case CallbackProto::kShowListPopup:
      if (proto.show_list_popup().item_names_model_identifier().empty()) {
        VLOG(1) << "Error creating ShowListPopup interaction: "
                   "items_list_model_identifier not set";
        return base::nullopt;
      }
      if (proto.show_list_popup()
              .selected_item_indices_model_identifier()
              .empty()) {
        VLOG(1) << "Error creating ShowListPopup interaction: "
                   "selected_item_indices_model_identifier not set";
        return base::nullopt;
      }
      return base::Optional<InteractionHandlerAndroid::InteractionCallback>(
          base::BindRepeating(&ShowListPopup, user_model->GetWeakPtr(),
                              proto.show_list_popup(), jcontext, jdelegate));
    case CallbackProto::kComputeValue:
      if (proto.compute_value().result_model_identifier().empty()) {
        VLOG(1) << "Error creating ComputeValue interaction: "
                   "result_model_identifier empty";
        return base::nullopt;
      }
      return base::Optional<InteractionHandlerAndroid::InteractionCallback>(
          base::BindRepeating(&TryComputeValue,
                              basic_interactions->GetWeakPtr(),
                              proto.compute_value()));
    case CallbackProto::kSetUserActions:
      if (proto.set_user_actions().model_identifier().empty()) {
        VLOG(1) << "Error creating SetUserActions interaction: "
                   "model_identifier not set";
        return base::nullopt;
      }
      return base::Optional<InteractionHandlerAndroid::InteractionCallback>(
          base::BindRepeating(&SetUserActionsIgnoreReturn,
                              basic_interactions->GetWeakPtr(),
                              proto.set_user_actions()));
    case CallbackProto::kEndAction:
      return base::Optional<InteractionHandlerAndroid::InteractionCallback>(
          base::BindRepeating(&TryEndAction, basic_interactions->GetWeakPtr(),
                              proto.end_action()));
    case CallbackProto::kShowCalendarPopup:
      if (proto.show_calendar_popup().date_model_identifier().empty()) {
        VLOG(1) << "Error creating ShowCalendarPopup interaction: "
                   "date_model_identifier not set";
        return base::nullopt;
      }
      return base::Optional<InteractionHandlerAndroid::InteractionCallback>(
          base::BindRepeating(&ShowCalendarPopup, user_model->GetWeakPtr(),
                              proto.show_calendar_popup(), jcontext,
                              jdelegate));
    case CallbackProto::kSetText:
      if (proto.set_text().model_identifier().empty()) {
        VLOG(1) << "Error creating SetText interaction: "
                   "model_identifier not set";
        return base::nullopt;
      }
      if (proto.set_text().view_identifier().empty()) {
        VLOG(1) << "Error creating SetText interaction: "
                   "view_identifier not set";
        return base::nullopt;
      }
      return base::Optional<InteractionHandlerAndroid::InteractionCallback>(
          base::BindRepeating(&SetTextViewText, user_model->GetWeakPtr(),
                              proto.set_text(), views));
      break;
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
      if (proto.toggle_user_action().enabled_model_identifier().empty()) {
        VLOG(1) << "Error creating ToggleUserAction interaction: "
                   "enabled_model_identifier not set";
        return base::nullopt;
      }
      return base::Optional<InteractionHandlerAndroid::InteractionCallback>(
          base::BindRepeating(&TryToggleUserAction,
                              basic_interactions->GetWeakPtr(),
                              proto.toggle_user_action()));
    case CallbackProto::KIND_NOT_SET:
      VLOG(1) << "Error creating interaction: kind not set";
      return base::nullopt;
  }
}

}  // namespace

InteractionHandlerAndroid::InteractionHandlerAndroid(
    EventHandler* event_handler,
    base::android::ScopedJavaLocalRef<jobject> jcontext)
    : event_handler_(event_handler) {
  DCHECK(jcontext);
  jcontext_ = base::android::ScopedJavaGlobalRef<jobject>(jcontext);
}

InteractionHandlerAndroid::~InteractionHandlerAndroid() {
  event_handler_->RemoveObserver(this);
}

void InteractionHandlerAndroid::StartListening() {
  is_listening_ = true;
  event_handler_->AddObserver(this);
}

void InteractionHandlerAndroid::StopListening() {
  event_handler_->RemoveObserver(this);
  is_listening_ = false;
}

bool InteractionHandlerAndroid::AddInteractionsFromProto(
    const InteractionsProto& proto,
    JNIEnv* env,
    std::map<std::string, base::android::ScopedJavaGlobalRef<jobject>>* views,
    base::android::ScopedJavaGlobalRef<jobject> jdelegate,
    UserModel* user_model,
    BasicInteractions* basic_interactions) {
  if (is_listening_) {
    NOTREACHED() << "Interactions can not be added while listening to events!";
    return false;
  }
  for (const auto& interaction_proto : proto.interactions()) {
    auto key = CreateEventKeyFromProto(interaction_proto.trigger_event(), env,
                                       views, jdelegate);
    if (!key) {
      VLOG(1) << "Invalid trigger event for interaction";
      return false;
    }

    for (const auto& callback_proto : interaction_proto.callbacks()) {
      auto callback = CreateInteractionCallbackFromProto(
          callback_proto, user_model, basic_interactions, views, jcontext_,
          jdelegate);
      if (!callback) {
        VLOG(1) << "Invalid callback for interaction";
        return false;
      }
      // Wrap callback in condition handler if necessary.
      if (callback_proto.has_condition_model_identifier()) {
        callback =
            base::Optional<InteractionHandlerAndroid::InteractionCallback>(
                base::BindRepeating(&TryRunConditionalCallback,
                                    basic_interactions->GetWeakPtr(),
                                    callback_proto.condition_model_identifier(),
                                    *callback));
      }
      AddInteraction(*key, *callback);
    }
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

}  // namespace autofill_assistant
