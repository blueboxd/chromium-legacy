// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/autofill_assistant/interaction_handler_android.h"
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
              const std::string& identifier,
              const ValueProto& value) {
  if (!user_model) {
    return;
  }
  user_model->SetValue(identifier, value);
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
          proto.on_view_clicked().has_value()
              ? ui_controller_android_utils::ToJavaValue(
                    env, proto.on_view_clicked().value())
              : nullptr,
          jdelegate);
      return base::Optional<EventHandler::EventKey>(
          {proto.kind_case(), proto.on_view_clicked().view_identifier()});
    }
    case EventProto::KIND_NOT_SET:
      return base::nullopt;
  }
}

base::Optional<InteractionHandlerAndroid::InteractionCallback>
CreateInteractionCallbackFromProto(const CallbackProto& proto,
                                   UserModel* user_model) {
  switch (proto.kind_case()) {
    case CallbackProto::kSetValue:
      if (proto.set_value().model_identifier().empty()) {
        DVLOG(1)
            << "Error creating SetValue interaction: model_identifier not set";
        return base::nullopt;
      }
      return base::Optional<InteractionHandlerAndroid::InteractionCallback>(
          base::BindRepeating(&SetValue, user_model->GetWeakPtr(),
                              proto.set_value().model_identifier()));
    case CallbackProto::KIND_NOT_SET:
      DVLOG(1) << "Error creating interaction: kind not set";
      return base::nullopt;
  }
}

}  // namespace

InteractionHandlerAndroid::InteractionHandlerAndroid(
    EventHandler* event_handler)
    : event_handler_(event_handler) {}
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
      auto callback =
          CreateInteractionCallbackFromProto(callback_proto, user_model);
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

void InteractionHandlerAndroid::OnEvent(const EventHandler::EventKey& key,
                                        const ValueProto& value) {
  auto it = interactions_.find(key);
  if (it != interactions_.end()) {
    for (auto& callback : it->second) {
      callback.Run(value);
    }
  }
}

}  // namespace autofill_assistant
