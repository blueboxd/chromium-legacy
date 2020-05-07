// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/autofill_assistant/generic_ui_controller_android.h"
#include "base/android/jni_string.h"
#include "chrome/android/features/autofill_assistant/jni_headers/AssistantDrawable_jni.h"
#include "chrome/android/features/autofill_assistant/jni_headers/AssistantViewFactory_jni.h"
#include "chrome/browser/android/autofill_assistant/assistant_generic_ui_delegate.h"
#include "chrome/browser/android/autofill_assistant/generic_ui_events_android.h"
#include "chrome/browser/android/autofill_assistant/generic_ui_interactions_android.h"
#include "chrome/browser/android/autofill_assistant/interaction_handler_android.h"
#include "chrome/browser/android/autofill_assistant/ui_controller_android_utils.h"
#include "components/autofill_assistant/browser/event_handler.h"
#include "components/autofill_assistant/browser/generic_ui_java_generated_enums.h"
#include "components/autofill_assistant/browser/ui_delegate.h"

namespace autofill_assistant {

namespace {

// Forward declaration to allow recursive calls.
base::android::ScopedJavaGlobalRef<jobject> CreateViewHierarchy(
    JNIEnv* env,
    const base::android::ScopedJavaLocalRef<jobject>& jcontext,
    const base::android::ScopedJavaGlobalRef<jobject>& jdelegate,
    const ViewProto& proto,
    InteractionHandlerAndroid* interaction_handler,
    std::map<std::string, base::android::ScopedJavaGlobalRef<jobject>>* views);

base::android::ScopedJavaLocalRef<jobject> CreateJavaDrawable(
    JNIEnv* env,
    const base::android::ScopedJavaLocalRef<jobject>& jcontext,
    const DrawableProto& proto) {
  switch (proto.drawable_case()) {
    case DrawableProto::kResourceIdentifier:
      if (!Java_AssistantDrawable_isValidDrawableResource(
              env, jcontext,
              base::android::ConvertUTF8ToJavaString(
                  env, proto.resource_identifier()))) {
        VLOG(1) << "Encountered invalid drawable resource identifier: "
                << proto.resource_identifier();
        return nullptr;
      }
      return Java_AssistantDrawable_createFromResource(
          env, base::android::ConvertUTF8ToJavaString(
                   env, proto.resource_identifier()));
      break;
    case DrawableProto::kBitmap: {
      int width_pixels = ui_controller_android_utils::GetPixelSizeOrDefault(
          env, jcontext, proto.bitmap().width(), 0);
      int height_pixels = ui_controller_android_utils::GetPixelSizeOrDefault(
          env, jcontext, proto.bitmap().height(), 0);
      return Java_AssistantDrawable_createFromUrl(
          env,
          base::android::ConvertUTF8ToJavaString(env, proto.bitmap().url()),
          width_pixels, height_pixels);
    }
    case DrawableProto::kShape: {
      switch (proto.shape().shape_case()) {
        case ShapeDrawableProto::kRectangle: {
          auto jbackground_color = ui_controller_android_utils::GetJavaColor(
              env, jcontext, proto.shape().background_color());
          auto jstroke_color = ui_controller_android_utils::GetJavaColor(
              env, jcontext, proto.shape().stroke_color());
          int stroke_width_pixels =
              ui_controller_android_utils::GetPixelSizeOrDefault(
                  env, jcontext, proto.shape().stroke_width(), 0);
          int corner_radius_pixels =
              ui_controller_android_utils::GetPixelSizeOrDefault(
                  env, jcontext, proto.shape().rectangle().corner_radius(), 0);
          return Java_AssistantDrawable_createRectangleShape(
              env, jbackground_color, jstroke_color, stroke_width_pixels,
              corner_radius_pixels);
          break;
        }
        case ShapeDrawableProto::SHAPE_NOT_SET:
          return nullptr;
      }
      break;
    }
    case DrawableProto::DRAWABLE_NOT_SET:
      return nullptr;
      break;
  }
}

base::android::ScopedJavaLocalRef<jobject> CreateJavaViewContainer(
    JNIEnv* env,
    const base::android::ScopedJavaLocalRef<jobject>& jcontext,
    const base::android::ScopedJavaLocalRef<jstring>& jidentifier,
    const ViewContainerProto& proto) {
  base::android::ScopedJavaLocalRef<jobject> jcontainer = nullptr;
  switch (proto.container_case()) {
    case ViewContainerProto::kLinearLayout:
      jcontainer = Java_AssistantViewFactory_createLinearLayout(
          env, jcontext, jidentifier, proto.linear_layout().orientation());
      break;
    case ViewContainerProto::kExpanderAccordion:
      jcontainer = Java_AssistantViewFactory_createVerticalExpanderAccordion(
          env, jcontext, jidentifier, proto.expander_accordion().orientation());
      break;
    case ViewContainerProto::CONTAINER_NOT_SET:
      return nullptr;
  }
  return jcontainer;
}

base::android::ScopedJavaLocalRef<jobject> CreateJavaTextView(
    JNIEnv* env,
    const base::android::ScopedJavaLocalRef<jobject>& jcontext,
    const base::android::ScopedJavaGlobalRef<jobject>& jdelegate,
    const base::android::ScopedJavaLocalRef<jstring>& jidentifier,
    const TextViewProto& proto) {
  base::android::ScopedJavaLocalRef<jstring> jtext_appearance = nullptr;
  if (proto.has_text_appearance()) {
    jtext_appearance =
        base::android::ConvertUTF8ToJavaString(env, proto.text_appearance());
  }
  return Java_AssistantViewFactory_createTextView(
      env, jcontext, jdelegate, jidentifier,
      base::android::ConvertUTF8ToJavaString(
          env, proto.has_text() ? proto.text() : std::string()),
      jtext_appearance, proto.text_alignment());
}

base::android::ScopedJavaLocalRef<jobject> CreateJavaVerticalExpander(
    JNIEnv* env,
    const base::android::ScopedJavaLocalRef<jobject>& jcontext,
    const base::android::ScopedJavaGlobalRef<jobject>& jdelegate,
    const base::android::ScopedJavaLocalRef<jstring>& jidentifier,
    const VerticalExpanderViewProto& proto,
    InteractionHandlerAndroid* interaction_handler,
    std::map<std::string, base::android::ScopedJavaGlobalRef<jobject>>* views) {
  base::android::ScopedJavaGlobalRef<jobject> jtitle_view = nullptr;
  if (proto.has_title_view()) {
    jtitle_view =
        CreateViewHierarchy(env, jcontext, jdelegate, proto.title_view(),
                            interaction_handler, views);
    if (!jtitle_view) {
      return nullptr;
    }
  }
  base::android::ScopedJavaGlobalRef<jobject> jcollapsed_view = nullptr;
  if (proto.has_collapsed_view()) {
    jcollapsed_view =
        CreateViewHierarchy(env, jcontext, jdelegate, proto.collapsed_view(),
                            interaction_handler, views);
    if (!jcollapsed_view) {
      return nullptr;
    }
  }
  base::android::ScopedJavaGlobalRef<jobject> jexpanded_view = nullptr;
  if (proto.has_expanded_view()) {
    jexpanded_view =
        CreateViewHierarchy(env, jcontext, jdelegate, proto.expanded_view(),
                            interaction_handler, views);
    if (!jexpanded_view) {
      return nullptr;
    }
  }
  VerticalExpanderChevronStyle chevron_style;
  switch (proto.chevron_style()) {
    case VerticalExpanderViewProto::NOT_SET_AUTOMATIC:
      chevron_style = VerticalExpanderChevronStyle::NOT_SET_AUTOMATIC;
      break;
    case VerticalExpanderViewProto::ALWAYS:
      chevron_style = VerticalExpanderChevronStyle::ALWAYS;
      break;
    case VerticalExpanderViewProto::NEVER:
      chevron_style = VerticalExpanderChevronStyle::NEVER;
      break;
    default:
      NOTREACHED();
      return nullptr;
  }
  return Java_AssistantViewFactory_createVerticalExpander(
      env, jcontext, jidentifier, jtitle_view, jcollapsed_view, jexpanded_view,
      static_cast<int>(chevron_style));
}

base::android::ScopedJavaLocalRef<jobject> CreateJavaToggleButton(
    JNIEnv* env,
    const base::android::ScopedJavaLocalRef<jobject>& jcontext,
    const base::android::ScopedJavaGlobalRef<jobject>& jdelegate,
    const base::android::ScopedJavaLocalRef<jstring>& jidentifier,
    const ToggleButtonViewProto& proto,
    InteractionHandlerAndroid* interaction_handler,
    std::map<std::string, base::android::ScopedJavaGlobalRef<jobject>>* views) {
  if (proto.model_identifier().empty()) {
    VLOG(1) << "Failed to create ToggleButtonViewProto: model_identifier not "
               "specified";
    return nullptr;
  }
  if (proto.kind_case() == ToggleButtonViewProto::KIND_NOT_SET) {
    VLOG(1) << "Failed to create ToggleButtonViewProto: kind not set";
    return nullptr;
  }

  base::android::ScopedJavaGlobalRef<jobject> jcontent_left_view = nullptr;
  if (proto.has_left_content_view()) {
    jcontent_left_view =
        CreateViewHierarchy(env, jcontext, jdelegate, proto.left_content_view(),
                            interaction_handler, views);
    if (!jcontent_left_view) {
      return nullptr;
    }
  }
  base::android::ScopedJavaGlobalRef<jobject> jcontent_right_view = nullptr;
  if (proto.has_right_content_view()) {
    jcontent_right_view = CreateViewHierarchy(env, jcontext, jdelegate,
                                              proto.right_content_view(),
                                              interaction_handler, views);
    if (!jcontent_right_view) {
      return nullptr;
    }
  }
  switch (proto.kind_case()) {
    case ToggleButtonViewProto::kCheckBox:
    case ToggleButtonViewProto::kRadioButton:
      return Java_AssistantViewFactory_createToggleButton(
          env, jcontext, jdelegate, jidentifier, jcontent_left_view,
          jcontent_right_view,
          proto.kind_case() == ToggleButtonViewProto::kCheckBox,
          base::android::ConvertUTF8ToJavaString(env,
                                                 proto.model_identifier()));
    case ToggleButtonViewProto::KIND_NOT_SET:
      NOTREACHED();
      return nullptr;
  }
  return nullptr;
}

base::android::ScopedJavaGlobalRef<jobject> CreateJavaView(
    JNIEnv* env,
    const base::android::ScopedJavaLocalRef<jobject>& jcontext,
    const base::android::ScopedJavaGlobalRef<jobject>& jdelegate,
    const ViewProto& proto,
    InteractionHandlerAndroid* interaction_handler,
    std::map<std::string, base::android::ScopedJavaGlobalRef<jobject>>* views) {
  auto jidentifier =
      base::android::ConvertUTF8ToJavaString(env, proto.identifier());
  base::android::ScopedJavaLocalRef<jobject> jview = nullptr;
  switch (proto.view_case()) {
    case ViewProto::kViewContainer:
      jview = CreateJavaViewContainer(env, jcontext, jidentifier,
                                      proto.view_container());
      break;
    case ViewProto::kTextView:
      jview = CreateJavaTextView(env, jcontext, jdelegate, jidentifier,
                                 proto.text_view());
      break;
    case ViewProto::kDividerView:
      jview = Java_AssistantViewFactory_createDividerView(env, jcontext,
                                                          jidentifier);
      break;
    case ViewProto::kImageView: {
      auto jimage =
          CreateJavaDrawable(env, jcontext, proto.image_view().image());
      if (!jimage) {
        VLOG(1) << "Failed to create image for " << proto.identifier();
        return nullptr;
      }
      jview = Java_AssistantViewFactory_createImageView(env, jcontext,
                                                        jidentifier, jimage);
      break;
    }
    case ViewProto::kVerticalExpanderView: {
      jview = CreateJavaVerticalExpander(env, jcontext, jdelegate, jidentifier,
                                         proto.vertical_expander_view(),
                                         interaction_handler, views);
      break;
    }
    case ViewProto::kTextInputView: {
      if (proto.text_input_view().model_identifier().empty()) {
        VLOG(1) << "Failed to create text input view '" << proto.identifier()
                << "': model_identifier not set";
        return nullptr;
      }
      jview = Java_AssistantViewFactory_createTextInputView(
          env, jcontext, jdelegate, jidentifier,
          static_cast<int>(proto.text_input_view().type()),
          base::android::ConvertUTF8ToJavaString(
              env, proto.text_input_view().hint()),
          base::android::ConvertUTF8ToJavaString(
              env, proto.text_input_view().model_identifier()));
      break;
    }
    case ViewProto::kToggleButtonView:
      jview = CreateJavaToggleButton(env, jcontext, jdelegate, jidentifier,
                                     proto.toggle_button_view(),
                                     interaction_handler, views);
      break;
    case ViewProto::VIEW_NOT_SET:
      NOTREACHED();
      return nullptr;
  }
  if (!jview) {
    return nullptr;
  }

  if (proto.has_attributes()) {
    Java_AssistantViewFactory_setViewAttributes(
        env, jview, jcontext, proto.attributes().padding_start(),
        proto.attributes().padding_top(), proto.attributes().padding_end(),
        proto.attributes().padding_bottom(),
        CreateJavaDrawable(env, jcontext, proto.attributes().background()),
        proto.attributes().has_content_description()
            ? base::android::ConvertUTF8ToJavaString(
                  env, proto.attributes().content_description())
            : nullptr,
        proto.attributes().visible(), proto.attributes().enabled());
  }
  if (proto.has_layout_params()) {
    Java_AssistantViewFactory_setViewLayoutParams(
        env, jview, jcontext, proto.layout_params().layout_width(),
        proto.layout_params().layout_height(),
        proto.layout_params().layout_weight(),
        proto.layout_params().margin_start(),
        proto.layout_params().margin_top(), proto.layout_params().margin_end(),
        proto.layout_params().margin_bottom(),
        proto.layout_params().layout_gravity(),
        proto.layout_params().minimum_width(),
        proto.layout_params().minimum_height());
  }

  return base::android::ScopedJavaGlobalRef<jobject>(jview);
}

bool CreateImplicitInteractionsForView(
    const ViewProto& proto,
    InteractionHandlerAndroid* interaction_handler,
    std::map<std::string, base::android::ScopedJavaGlobalRef<jobject>>* views) {
  switch (proto.view_case()) {
    case ViewProto::kTextInputView: {
      // Auto-update the text of the view whenever the corresponding value in
      // the model changes.
      InteractionProto implicit_set_text_interaction;
      implicit_set_text_interaction.mutable_trigger_event()
          ->mutable_on_value_changed()
          ->set_model_identifier(proto.text_input_view().model_identifier());
      SetTextProto set_text_callback;
      set_text_callback.mutable_text()->set_model_identifier(
          proto.text_input_view().model_identifier());
      set_text_callback.set_view_identifier(proto.identifier());
      *implicit_set_text_interaction.add_callbacks()->mutable_set_text() =
          set_text_callback;

      if (!interaction_handler->AddInteractionsFromProto(
              implicit_set_text_interaction)) {
        VLOG(1) << "Failed to create implicit SetText interaction for "
                << proto.identifier();
        return false;
      }
      break;
    }
    case ViewProto::kTextView: {
      if (proto.text_view().model_identifier().empty()) {
        break;
      }
      // Auto-update text view content.
      InteractionProto implicit_set_text_interaction;
      implicit_set_text_interaction.mutable_trigger_event()
          ->mutable_on_value_changed()
          ->set_model_identifier(proto.text_view().model_identifier());
      SetTextProto set_text_callback;
      set_text_callback.mutable_text()->set_model_identifier(
          proto.text_view().model_identifier());
      set_text_callback.set_view_identifier(proto.identifier());
      *implicit_set_text_interaction.add_callbacks()->mutable_set_text() =
          set_text_callback;

      if (!interaction_handler->AddInteractionsFromProto(
              implicit_set_text_interaction)) {
        VLOG(1) << "Failed to create implicit SetText interaction for "
                << proto.identifier();
        return false;
      }
      break;
    }
    case ViewProto::kToggleButtonView: {
      if (proto.identifier().empty()) {
        VLOG(1) << "Failed to create toggle button: view_identifier not set, "
                   "but mandatory for toggle buttons";
        return false;
      }
      // Auto-update toggle state.
      auto model_identifier = proto.toggle_button_view().model_identifier();
      auto toggle_callback =
          base::BindRepeating(&android_interactions::SetToggleButtonChecked,
                              interaction_handler->GetUserModel()->GetWeakPtr(),
                              proto.identifier(), model_identifier, views);
      interaction_handler->AddInteraction(
          {EventProto::kOnValueChanged, model_identifier}, toggle_callback);
      if (proto.toggle_button_view().kind_case() !=
          ToggleButtonViewProto::kRadioButton) {
        break;
      }
      auto radio_group =
          proto.toggle_button_view().radio_button().radio_group_identifier();
      interaction_handler->AddRadioButtonToGroup(radio_group, model_identifier);

      // De-select all other radio buttons whenever |model_identifier| is set to
      // true.
      auto radio_callback = base::BindRepeating(
          &InteractionHandlerAndroid::UpdateRadioButtonGroup,
          interaction_handler->GetWeakPtr(), radio_group, model_identifier);
      radio_callback = base::BindRepeating(
          &android_interactions::RunConditionalCallback,
          interaction_handler->GetBasicInteractions()->GetWeakPtr(),
          model_identifier, radio_callback);
      interaction_handler->AddInteraction(
          {EventProto::kOnValueChanged, model_identifier}, radio_callback);
      break;
    }
    case ViewProto::kViewContainer:
    case ViewProto::kVerticalExpanderView:
    case ViewProto::kDividerView:
    case ViewProto::kImageView:
      // Nothing to do, no implicit interactions necessary.
      break;
    case ViewProto::VIEW_NOT_SET:
      NOTREACHED();
      return false;
  }

  return true;
}

// Recursively runs through all views defined in |proto| in a depth-first
// manner and inflates and configures each view. Implicit interactions will be
// added to |interaction_handler|, and views with identifiers will be added to
// the |views| lookup map. Returns the root of the created java view hierarchy
// or null in case of error.
base::android::ScopedJavaGlobalRef<jobject> CreateViewHierarchy(
    JNIEnv* env,
    const base::android::ScopedJavaLocalRef<jobject>& jcontext,
    const base::android::ScopedJavaGlobalRef<jobject>& jdelegate,
    const ViewProto& proto,
    InteractionHandlerAndroid* interaction_handler,
    std::map<std::string, base::android::ScopedJavaGlobalRef<jobject>>* views) {
  auto jview = CreateJavaView(env, jcontext, jdelegate, proto,
                              interaction_handler, views);
  if (!jview) {
    VLOG(1) << "View inflation failed for '" << proto.identifier() << "'";
    return nullptr;
  }
  if (proto.view_case() == ViewProto::kViewContainer) {
    for (const auto& child : proto.view_container().views()) {
      auto jchild = CreateViewHierarchy(env, jcontext, jdelegate, child,
                                        interaction_handler, views);
      if (!jchild) {
        return nullptr;
      }
      Java_AssistantViewFactory_addViewToContainer(env, jview, jchild);
    }
  }

  if (!CreateImplicitInteractionsForView(proto, interaction_handler, views)) {
    VLOG(1) << "Implicit interaction creation failed for '"
            << proto.identifier() << "'";
    return nullptr;
  }

  if (!proto.identifier().empty()) {
    DCHECK(views->find(proto.identifier()) == views->end());
    views->emplace(proto.identifier(), jview);
  }

  return jview;
}

}  // namespace

GenericUiControllerAndroid::GenericUiControllerAndroid(
    base::android::ScopedJavaGlobalRef<jobject> jroot_view,
    std::unique_ptr<
        std::map<std::string, base::android::ScopedJavaGlobalRef<jobject>>>
        views,
    std::unique_ptr<InteractionHandlerAndroid> interaction_handler)
    : jroot_view_(jroot_view),
      views_(std::move(views)),
      interaction_handler_(std::move(interaction_handler)) {}

GenericUiControllerAndroid::~GenericUiControllerAndroid() {
  interaction_handler_->StopListening();
}

// static
std::unique_ptr<GenericUiControllerAndroid>
GenericUiControllerAndroid::CreateFromProto(
    const GenericUserInterfaceProto& proto,
    base::android::ScopedJavaGlobalRef<jobject> jcontext,
    base::android::ScopedJavaGlobalRef<jobject> jdelegate,
    EventHandler* event_handler,
    UserModel* user_model,
    BasicInteractions* basic_interactions) {
  // Create view layout.
  auto views = std::make_unique<
      std::map<std::string, base::android::ScopedJavaGlobalRef<jobject>>>();
  auto interaction_handler = std::make_unique<InteractionHandlerAndroid>(
      event_handler, user_model, basic_interactions, views.get(), jcontext,
      jdelegate);
  JNIEnv* env = base::android::AttachCurrentThread();
  auto jroot_view =
      proto.has_root_view()
          ? CreateViewHierarchy(
                env, base::android::ScopedJavaLocalRef<jobject>(jcontext),
                jdelegate, proto.root_view(), interaction_handler.get(),
                views.get())
          : nullptr;

  // Create proto interactions (i.e., native -> java).
  for (const auto& interaction : proto.interactions().interactions()) {
    if (!interaction_handler->AddInteractionsFromProto(interaction)) {
      return nullptr;
    }
  }

  // Create java listeners (i.e., java -> native).
  if (!android_events::CreateJavaListenersFromProto(env, views.get(), jdelegate,
                                                    proto.interactions())) {
    return nullptr;
  }

  // Set initial state.
  interaction_handler->StartListening();
  user_model->MergeWithProto(proto.model(),
                             /*force_notifications=*/true);

  return std::make_unique<GenericUiControllerAndroid>(
      jroot_view, std::move(views), std::move(interaction_handler));
}

}  // namespace autofill_assistant
