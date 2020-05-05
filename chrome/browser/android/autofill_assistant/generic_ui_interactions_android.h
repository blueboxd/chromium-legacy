// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_GENERIC_UI_INTERACTIONS_ANDROID_H_
#define CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_GENERIC_UI_INTERACTIONS_ANDROID_H_

#include <string>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/android/autofill_assistant/interaction_handler_android.h"
#include "components/autofill_assistant/browser/basic_interactions.h"
#include "components/autofill_assistant/browser/generic_ui.pb.h"

namespace autofill_assistant {
namespace android_interactions {

// Writes a value to the model.
void SetValue(base::WeakPtr<BasicInteractions> basic_interactions,
              const SetModelValueProto& proto);

// Computes a value and writes it to the model.
void ComputeValue(base::WeakPtr<BasicInteractions> basic_interactions,
                  const ComputeValueProto& proto);

// Sets the list of available user actions (i.e., chips and direct actions).
void SetUserActions(base::WeakPtr<BasicInteractions> basic_interactions,
                    const SetUserActionsProto& proto);

// Ends the current ShowGenericUi action.
void EndAction(base::WeakPtr<BasicInteractions> basic_interactions,
               bool view_inflation_successful,
               const EndActionProto& proto);

// Enables or disables a particular user action.
void ToggleUserAction(base::WeakPtr<BasicInteractions> basic_interactions,
                      const ToggleUserActionProto& proto);

// Displays an info popup on the screen.
void ShowInfoPopup(const InfoPopupProto& proto,
                   base::android::ScopedJavaGlobalRef<jobject> jcontext);

// Displays a list popup on the screen.
void ShowListPopup(base::WeakPtr<UserModel> user_model,
                   const ShowListPopupProto& proto,
                   base::android::ScopedJavaGlobalRef<jobject> jcontext,
                   base::android::ScopedJavaGlobalRef<jobject> jdelegate);

// Displays a calendar popup on the screen.
void ShowCalendarPopup(base::WeakPtr<UserModel> user_model,
                       const ShowCalendarPopupProto& proto,
                       base::android::ScopedJavaGlobalRef<jobject> jcontext,
                       base::android::ScopedJavaGlobalRef<jobject> jdelegate);

// Displays a generic popup on the screen.
void ShowGenericPopup(const ShowGenericUiPopupProto& proto,
                      base::android::ScopedJavaGlobalRef<jobject> jcontent_view,
                      base::android::ScopedJavaGlobalRef<jobject> jcontext,
                      base::android::ScopedJavaGlobalRef<jobject> jdelegate);

// Sets the text of a view.
void SetViewText(
    base::WeakPtr<UserModel> user_model,
    const SetTextProto& proto,
    std::map<std::string, base::android::ScopedJavaGlobalRef<jobject>>* views,
    base::android::ScopedJavaGlobalRef<jobject> jdelegate);

// Sets the visibility of a view.
void SetViewVisibility(
    base::WeakPtr<UserModel> user_model,
    const SetViewVisibilityProto& proto,
    std::map<std::string, base::android::ScopedJavaGlobalRef<jobject>>* views);

// Enables or disables a view.
void SetViewEnabled(
    base::WeakPtr<UserModel> user_model,
    const SetViewEnabledProto& proto,
    std::map<std::string, base::android::ScopedJavaGlobalRef<jobject>>* views);

// A simple wrapper around a basic interaction, needed because we can't directly
// bind a repeating callback to a method with non-void return value.
void RunConditionalCallback(
    base::WeakPtr<BasicInteractions> basic_interactions,
    const std::string& condition_identifier,
    InteractionHandlerAndroid::InteractionCallback callback);

// Sets the checked state of a toggle button.
void SetToggleButtonChecked(
    base::WeakPtr<UserModel> user_model,
    const std::string& view_identifier,
    const std::string& model_identifier,
    std::map<std::string, base::android::ScopedJavaGlobalRef<jobject>>* views);

}  // namespace android_interactions
}  // namespace autofill_assistant

#endif  // CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_GENERIC_UI_INTERACTIONS_ANDROID_H_
