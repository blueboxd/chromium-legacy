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
#include "components/autofill_assistant/browser/user_model.h"

namespace autofill_assistant {

namespace {

void SetValue(base::WeakPtr<UserModel> user_model,
              const SetModelValueProto& proto) {
  if (!user_model) {
    return;
  }
  user_model->SetValue(proto.model_identifier(), proto.value());
}

void ComputeValueBooleanAnd(base::WeakPtr<UserModel> user_model,
                            const BooleanAndProto& proto,
                            const std::string& result_model_identifier) {
  if (!user_model) {
    return;
  }

  auto values = user_model->GetValues(proto.model_identifiers());
  if (!values.has_value()) {
    DVLOG(2) << "Failed to find values in user model";
    return;
  }

  if (!AreAllValuesOfType(*values, ValueProto::kBooleans) ||
      !AreAllValuesOfSize(*values, 1)) {
    DVLOG(2) << "All values must be 'boolean' and contain exactly 1 value each";
    return;
  }

  bool result = true;
  for (const auto& value : *values) {
    result &= value.booleans().values(0);
  }
  user_model->SetValue(result_model_identifier, SimpleValue(result));
}

void ComputeValueBooleanOr(base::WeakPtr<UserModel> user_model,
                           const BooleanOrProto& proto,
                           const std::string& result_model_identifier) {
  if (!user_model) {
    return;
  }

  auto values = user_model->GetValues(proto.model_identifiers());
  if (!values.has_value()) {
    DVLOG(2) << "Failed to find values in user model";
    return;
  }

  if (!AreAllValuesOfType(*values, ValueProto::kBooleans) ||
      !AreAllValuesOfSize(*values, 1)) {
    DVLOG(2) << "All values must be 'boolean' and contain exactly 1 value each";
    return;
  }

  bool result = true;
  for (const auto& value : *values) {
    result |= value.booleans().values(0);
  }
  user_model->SetValue(result_model_identifier, SimpleValue(result));
}

void ComputeValueBooleanNot(base::WeakPtr<UserModel> user_model,
                            const BooleanNotProto& proto,
                            const std::string& result_model_identifier) {
  if (!user_model) {
    return;
  }

  auto value = user_model->GetValue(proto.model_identifier());
  if (!value.has_value()) {
    DVLOG(2) << "Error evaluating " << __func__ << ": "
             << proto.model_identifier() << " not found in model";
    return;
  }
  if (value->booleans().values().size() != 1) {
    DVLOG(2) << "Error evaluating " << __func__
             << ": expected single boolean, but got " << *value;
    return;
  }

  user_model->SetValue(result_model_identifier,
                       SimpleValue(!value->booleans().values(0)));
}

base::Optional<InteractionHandlerAndroid::InteractionCallback>
CreateComputeValueCallbackForProto(base::WeakPtr<UserModel> user_model,
                                   const ComputeValueProto& proto) {
  if (proto.result_model_identifier().empty()) {
    DVLOG(1) << "Error creating ComputeValue interaction: "
                "result_model_identifier empty";
    return base::nullopt;
  }

  switch (proto.kind_case()) {
    case ComputeValueProto::kBooleanAnd:
      if (proto.boolean_and().model_identifiers().size() == 0) {
        DVLOG(1) << "Error creating ComputeValue::BooleanAnd interaction: no "
                    "model_identifiers specified";
        return base::nullopt;
      }
      return base::Optional<InteractionHandlerAndroid::InteractionCallback>(
          base::BindRepeating(&ComputeValueBooleanAnd, user_model,
                              proto.boolean_and(),
                              proto.result_model_identifier()));
    case ComputeValueProto::kBooleanOr:
      if (proto.boolean_or().model_identifiers().size() == 0) {
        DVLOG(1) << "Error creating ComputeValue::BooleanOr interaction: no "
                    "model_identifiers specified";
        return base::nullopt;
      }
      return base::Optional<InteractionHandlerAndroid::InteractionCallback>(
          base::BindRepeating(&ComputeValueBooleanOr, user_model,
                              proto.boolean_or(),
                              proto.result_model_identifier()));
    case ComputeValueProto::kBooleanNot:
      if (proto.boolean_not().model_identifier().empty()) {
        DVLOG(1) << "Error creating ComputeValue::BooleanNot interaction: "
                    "model_identifier not specified";
        return base::nullopt;
      }
      return base::Optional<InteractionHandlerAndroid::InteractionCallback>(
          base::BindRepeating(&ComputeValueBooleanNot, user_model,
                              proto.boolean_not(),
                              proto.result_model_identifier()));
    case ComputeValueProto::KIND_NOT_SET:
      DVLOG(1) << "Error creating ComputeValue interaction: kind not set";
      return base::nullopt;
  }

  return base::nullopt;
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
  auto jidentifier = base::android::ConvertUTF8ToJavaString(
      env, proto.selected_item_indices_model_identifier());

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
      proto.allow_multiselect(), jidentifier, jdelegate);
}

base::Optional<EventHandler::EventKey> CreateEventKeyFromProto(
    const EventProto& proto,
    JNIEnv* env,
    const std::map<std::string, base::android::ScopedJavaGlobalRef<jobject>>&
        views,
    base::android::ScopedJavaGlobalRef<jobject> jdelegate) {
  switch (proto.kind_case()) {
    case EventProto::kOnValueChanged:
      return base::Optional<EventHandler::EventKey>(
          {proto.kind_case(), proto.on_value_changed().model_identifier()});
    case EventProto::kOnViewClicked: {
      auto jview = views.find(proto.on_view_clicked().view_identifier());
      if (jview == views.end()) {
        LOG(ERROR) << "Invalid click event, no view with id='"
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
    case EventProto::KIND_NOT_SET:
      return base::nullopt;
  }
}

base::Optional<InteractionHandlerAndroid::InteractionCallback>
CreateInteractionCallbackFromProto(
    const CallbackProto& proto,
    UserModel* user_model,
    base::android::ScopedJavaGlobalRef<jobject> jcontext,
    base::android::ScopedJavaGlobalRef<jobject> jdelegate) {
  switch (proto.kind_case()) {
    case CallbackProto::kSetValue:
      if (proto.set_value().model_identifier().empty()) {
        DVLOG(1)
            << "Error creating SetValue interaction: model_identifier not set";
        return base::nullopt;
      }
      return base::Optional<InteractionHandlerAndroid::InteractionCallback>(
          base::BindRepeating(&SetValue, user_model->GetWeakPtr(),
                              proto.set_value()));
    case CallbackProto::kShowInfoPopup: {
      return base::Optional<InteractionHandlerAndroid::InteractionCallback>(
          base::BindRepeating(&ShowInfoPopup,
                              proto.show_info_popup().info_popup(), jcontext));
    }
    case CallbackProto::kShowListPopup:
      if (proto.show_list_popup().item_names_model_identifier().empty()) {
        DVLOG(1) << "Error creating ShowListPopup interaction: "
                    "items_list_model_identifier not set";
        return base::nullopt;
      }
      if (proto.show_list_popup()
              .selected_item_indices_model_identifier()
              .empty()) {
        DVLOG(1) << "Error creating ShowListPopup interaction: "
                    "selected_item_indices_model_identifier not set";
        return base::nullopt;
      }
      return base::Optional<InteractionHandlerAndroid::InteractionCallback>(
          base::BindRepeating(&ShowListPopup, user_model->GetWeakPtr(),
                              proto.show_list_popup(), jcontext, jdelegate));
    case CallbackProto::kComputeValue:
      return CreateComputeValueCallbackForProto(user_model->GetWeakPtr(),
                                                proto.compute_value());
    case CallbackProto::KIND_NOT_SET:
      DVLOG(1) << "Error creating interaction: kind not set";
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
    const std::map<std::string, base::android::ScopedJavaGlobalRef<jobject>>&
        views,
    base::android::ScopedJavaGlobalRef<jobject> jdelegate,
    UserModel* user_model) {
  if (is_listening_) {
    NOTREACHED() << "Interactions can not be added while listening to events!";
    return false;
  }
  for (const auto& interaction_proto : proto.interactions()) {
    auto key = CreateEventKeyFromProto(interaction_proto.trigger_event(), env,
                                       views, jdelegate);
    if (!key) {
      DVLOG(1) << "Invalid trigger event for interaction";
      return false;
    }

    for (const auto& callback_proto : interaction_proto.callbacks()) {
      auto callback = CreateInteractionCallbackFromProto(
          callback_proto, user_model, jcontext_, jdelegate);
      if (!callback) {
        DVLOG(1) << "Invalid callback for interaction";
        return false;
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
