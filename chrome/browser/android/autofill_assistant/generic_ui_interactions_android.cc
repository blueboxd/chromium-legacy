// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/autofill_assistant/generic_ui_interactions_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/optional.h"
#include "chrome/android/features/autofill_assistant/jni_headers/AssistantViewInteractions_jni.h"
#include "chrome/browser/android/autofill_assistant/generic_ui_controller_android.h"
#include "chrome/browser/android/autofill_assistant/ui_controller_android_utils.h"
#include "components/autofill_assistant/browser/user_model.h"

namespace autofill_assistant {
namespace android_interactions {

void SetValue(base::WeakPtr<BasicInteractions> basic_interactions,
              const SetModelValueProto& proto) {
  if (!basic_interactions) {
    return;
  }
  basic_interactions->SetValue(proto);
}

void ComputeValue(base::WeakPtr<BasicInteractions> basic_interactions,
                  const ComputeValueProto& proto) {
  if (!basic_interactions) {
    return;
  }
  basic_interactions->ComputeValue(proto);
}

void SetUserActions(base::WeakPtr<BasicInteractions> basic_interactions,
                    const SetUserActionsProto& proto) {
  if (!basic_interactions) {
    return;
  }
  basic_interactions->SetUserActions(proto);
}

void EndAction(base::WeakPtr<BasicInteractions> basic_interactions,
               bool view_inflation_successful,
               const EndActionProto& proto) {
  if (!basic_interactions) {
    return;
  }
  basic_interactions->EndAction(view_inflation_successful, proto);
}

void ToggleUserAction(base::WeakPtr<BasicInteractions> basic_interactions,
                      const ToggleUserActionProto& proto) {
  if (!basic_interactions) {
    return;
  }
  basic_interactions->ToggleUserAction(proto);
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

  auto item_names = user_model->GetValue(proto.item_names());
  if (!item_names.has_value()) {
    DVLOG(2) << "Failed to show list popup: '" << proto.item_names()
             << "' not found in model.";
    return;
  }
  if (item_names->strings().values().size() == 0) {
    DVLOG(2) << "Failed to show list popup: the list of item names in '"
             << proto.item_names() << "' was empty.";
    return;
  }

  base::Optional<ValueProto> item_types;
  if (proto.has_item_types()) {
    item_types = user_model->GetValue(proto.item_types());
    if (!item_types.has_value()) {
      DVLOG(2) << "Failed to show list popup: '" << proto.item_types()
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
  if (!initial_date.has_value()) {
    DVLOG(2) << "Failed to show calendar popup: "
             << proto.date_model_identifier() << " not found in model";
    return;
  }
  if (*initial_date != ValueProto() &&
      initial_date->dates().values().size() != 1) {
    DVLOG(2) << "Failed to show calendar popup: date_model_identifier must be "
                "empty or contain single date, but was "
             << *initial_date;
    return;
  }

  auto min_date = user_model->GetValue(proto.min_date());
  if (!min_date.has_value() || min_date->dates().values().size() != 1) {
    DVLOG(2) << "Failed to show calendar popup: min_date not found or invalid "
                "in user model at "
             << proto.min_date();
    return;
  }

  auto max_date = user_model->GetValue(proto.max_date());
  if (!max_date.has_value() || max_date->dates().values().size() != 1) {
    DVLOG(2) << "Failed to show calendar popup: max_date not found or invalid "
                "in user model at "
             << proto.max_date();
    return;
  }

  jboolean jsuccess = Java_AssistantViewInteractions_showCalendarPopup(
      env, jcontext,
      *initial_date != ValueProto()
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

void ShowGenericPopup(const ShowGenericUiPopupProto& proto,
                      base::android::ScopedJavaGlobalRef<jobject> jcontent_view,
                      base::android::ScopedJavaGlobalRef<jobject> jcontext,
                      base::android::ScopedJavaGlobalRef<jobject> jdelegate) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AssistantViewInteractions_showGenericPopup(
      env, jcontent_view, jcontext, jdelegate,
      base::android::ConvertUTF8ToJavaString(env, proto.popup_identifier()));
}

void SetViewText(
    base::WeakPtr<UserModel> user_model,
    const SetTextProto& proto,
    std::map<std::string, base::android::ScopedJavaGlobalRef<jobject>>* views,
    base::android::ScopedJavaGlobalRef<jobject> jdelegate) {
  if (!user_model) {
    return;
  }

  auto text = user_model->GetValue(proto.text());
  if (!text.has_value()) {
    DVLOG(2) << "Failed to set text for " << proto.view_identifier() << ": "
             << proto.text() << " not found in model";
    return;
  }
  if (text->strings().values_size() != 1) {
    DVLOG(2) << "Failed to set text for " << proto.view_identifier()
             << ": expected " << proto.text()
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
  Java_AssistantViewInteractions_setViewText(
      env, jview->second,
      base::android::ConvertUTF8ToJavaString(env, text->strings().values(0)),
      jdelegate);
}

void SetViewVisibility(
    base::WeakPtr<UserModel> user_model,
    const SetViewVisibilityProto& proto,
    std::map<std::string, base::android::ScopedJavaGlobalRef<jobject>>* views) {
  if (!user_model) {
    return;
  }

  auto jview = views->find(proto.view_identifier());
  if (jview == views->end()) {
    DVLOG(2) << "Failed to set view visibility for " << proto.view_identifier()
             << ": view not found";
    return;
  }

  auto visible_value = user_model->GetValue(proto.visible());
  if (!visible_value.has_value() ||
      visible_value->booleans().values_size() != 1) {
    DVLOG(2) << "Failed to set view visibility for " << proto.view_identifier()
             << ": " << proto.visible() << " did not contain single boolean";
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AssistantViewInteractions_setViewVisibility(
      env, jview->second,
      ui_controller_android_utils::ToJavaValue(env, *visible_value));
}

void SetViewEnabled(
    base::WeakPtr<UserModel> user_model,
    const SetViewEnabledProto& proto,
    std::map<std::string, base::android::ScopedJavaGlobalRef<jobject>>* views) {
  if (!user_model) {
    return;
  }

  auto jview = views->find(proto.view_identifier());
  if (jview == views->end()) {
    DVLOG(2) << "Failed to enable/disable view " << proto.view_identifier()
             << ": view not found";
    return;
  }

  auto enabled_value = user_model->GetValue(proto.enabled());
  if (!enabled_value.has_value() ||
      enabled_value->booleans().values_size() != 1) {
    DVLOG(2) << "Failed to enable/disable view " << proto.view_identifier()
             << ": " << proto.enabled() << " did not contain single boolean";
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AssistantViewInteractions_setViewEnabled(
      env, jview->second,
      ui_controller_android_utils::ToJavaValue(env, *enabled_value));
}

void RunConditionalCallback(
    base::WeakPtr<BasicInteractions> basic_interactions,
    const std::string& condition_identifier,
    InteractionHandlerAndroid::InteractionCallback callback) {
  if (!basic_interactions) {
    return;
  }
  basic_interactions->RunConditionalCallback(condition_identifier, callback);
}

void SetToggleButtonChecked(
    base::WeakPtr<UserModel> user_model,
    const std::string& view_identifier,
    const std::string& model_identifier,
    std::map<std::string, base::android::ScopedJavaGlobalRef<jobject>>* views) {
  if (!user_model) {
    return;
  }

  auto jview = views->find(view_identifier);
  if (jview == views->end()) {
    DVLOG(2) << "Failed to set toggle state for " << view_identifier
             << ": view not found";
    return;
  }

  auto checked_value = user_model->GetValue(model_identifier);
  if (!checked_value.has_value() ||
      checked_value->booleans().values_size() != 1) {
    DVLOG(2) << "Failed to set toggle state for " << view_identifier << ": "
             << model_identifier << " did not contain single boolean";
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  if (!Java_AssistantViewInteractions_setToggleButtonChecked(
          env, jview->second,
          ui_controller_android_utils::ToJavaValue(env, *checked_value))) {
    DVLOG(2) << "Failed to set toggle state for " << view_identifier
             << ": JNI call failed";
  }
}

}  // namespace android_interactions
}  // namespace autofill_assistant
