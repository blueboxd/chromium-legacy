// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/autofill_assistant/generic_ui_controller_android.h"
#include "base/android/jni_string.h"
#include "chrome/android/features/autofill_assistant/jni_headers/AssistantDrawable_jni.h"
#include "chrome/android/features/autofill_assistant/jni_headers/AssistantViewFactory_jni.h"
#include "chrome/browser/android/autofill_assistant/assistant_generic_ui_delegate.h"
#include "chrome/browser/android/autofill_assistant/interaction_handler_android.h"
#include "chrome/browser/android/autofill_assistant/ui_controller_android_utils.h"
#include "components/autofill_assistant/browser/event_handler.h"
#include "components/autofill_assistant/browser/ui_delegate.h"

namespace autofill_assistant {

namespace {

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
    case ViewContainerProto::CONTAINER_NOT_SET:
      return nullptr;
  }
  return jcontainer;
}

base::android::ScopedJavaLocalRef<jobject> CreateJavaTextView(
    JNIEnv* env,
    const base::android::ScopedJavaLocalRef<jobject>& jcontext,
    const base::android::ScopedJavaLocalRef<jstring>& jidentifier,
    const TextViewProto& proto) {
  base::android::ScopedJavaLocalRef<jstring> jtext_appearance = nullptr;
  if (proto.has_text_appearance()) {
    jtext_appearance =
        base::android::ConvertUTF8ToJavaString(env, proto.text_appearance());
  }
  return Java_AssistantViewFactory_createTextView(
      env, jcontext, jidentifier,
      base::android::ConvertUTF8ToJavaString(env, proto.text()),
      jtext_appearance);
}

// Forward declaration to allow recursive calls.
base::android::ScopedJavaGlobalRef<jobject> CreateJavaView(
    JNIEnv* env,
    const base::android::ScopedJavaLocalRef<jobject>& jcontext,
    const base::android::ScopedJavaGlobalRef<jobject>& jdelegate,
    const ViewProto& proto,
    std::map<std::string, base::android::ScopedJavaGlobalRef<jobject>>* views);

base::android::ScopedJavaGlobalRef<jobject> CreateJavaView(
    JNIEnv* env,
    const base::android::ScopedJavaLocalRef<jobject>& jcontext,
    const base::android::ScopedJavaGlobalRef<jobject>& jdelegate,
    const ViewProto& proto,
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
      jview = CreateJavaTextView(env, jcontext, jidentifier, proto.text_view());
      break;
    case ViewProto::kDividerView:
      jview = Java_AssistantViewFactory_createDividerView(env, jcontext,
                                                          jidentifier);
      break;
    case ViewProto::kImageView: {
      auto jimage =
          CreateJavaDrawable(env, jcontext, proto.image_view().image());
      if (!jimage) {
        LOG(ERROR) << "Failed to create image for " << proto.identifier();
        return nullptr;
      }
      jview = Java_AssistantViewFactory_createImageView(env, jcontext,
                                                        jidentifier, jimage);
      break;
    }
    case ViewProto::VIEW_NOT_SET:
      NOTREACHED();
      return nullptr;
  }
  if (!jview) {
    LOG(ERROR) << "Failed to create view " << proto.identifier();
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
        proto.attributes().visible());
  }
  if (proto.has_layout_params()) {
    Java_AssistantViewFactory_setViewLayoutParams(
        env, jview, jcontext, proto.layout_params().layout_width(),
        proto.layout_params().layout_height(),
        proto.layout_params().layout_weight(),
        proto.layout_params().margin_start(),
        proto.layout_params().margin_top(), proto.layout_params().margin_end(),
        proto.layout_params().margin_bottom(),
        proto.layout_params().layout_gravity());
  }

  if (proto.view_case() == ViewProto::kViewContainer) {
    for (const auto& child : proto.view_container().views()) {
      auto jchild = CreateJavaView(env, jcontext, jdelegate, child, views);
      if (!jchild) {
        return nullptr;
      }
      Java_AssistantViewFactory_addViewToContainer(env, jview, jchild);
    }
  }

  if (!jview) {
    return nullptr;
  }

  auto jview_global_ref = base::android::ScopedJavaGlobalRef<jobject>(jview);
  if (!proto.identifier().empty()) {
    DCHECK(views->find(proto.identifier()) == views->end());
    views->emplace(proto.identifier(), jview_global_ref);
  }
  return jview_global_ref;
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
    base::android::ScopedJavaLocalRef<jobject> jcontext,
    base::android::ScopedJavaGlobalRef<jobject> jdelegate,
    EventHandler* event_handler,
    UserModel* user_model,
    BasicInteractions* basic_interactions) {
  // Create view layout.
  JNIEnv* env = base::android::AttachCurrentThread();
  auto views = std::make_unique<
      std::map<std::string, base::android::ScopedJavaGlobalRef<jobject>>>();
  auto jroot_view = proto.has_root_view()
                        ? CreateJavaView(env, jcontext, jdelegate,
                                         proto.root_view(), views.get())
                        : nullptr;

  // Create interactions.
  auto interaction_handler =
      std::make_unique<InteractionHandlerAndroid>(event_handler, jcontext);
  if (!interaction_handler->AddInteractionsFromProto(
          proto.interactions(), env, views.get(), jdelegate, user_model,
          basic_interactions)) {
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
