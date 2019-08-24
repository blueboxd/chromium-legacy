// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_message_handler_android.h"

#include <memory>
#include <string>

#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/android/chrome_jni_headers/SharedClipboardMessageHandler_jni.h"
#include "chrome/browser/sharing/proto/shared_clipboard_message.pb.h"
#include "chrome/browser/sharing/proto/sharing_message.pb.h"
#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/sharing_send_message_result.h"
#include "chrome/browser/sharing/sharing_service.h"
#include "components/sync_device_info/device_info.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

SharedClipboardMessageHandler::SharedClipboardMessageHandler(
    SharingService* sharing_service)
    : sharing_service_(sharing_service) {}

SharedClipboardMessageHandler::~SharedClipboardMessageHandler() = default;

void SharedClipboardMessageHandler::OnMessage(
    const chrome_browser_sharing::SharingMessage& message) {
  DCHECK(message.has_shared_clipboard_message());

  ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
      .WriteText(base::UTF8ToUTF16(message.shared_clipboard_message().text()));

  std::string device_name;
  std::unique_ptr<syncer::DeviceInfo> device =
      sharing_service_->GetDeviceByGuid(message.sender_guid());
  if (device)
    device_name = device->client_name();

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SharedClipboardMessageHandler_showNotification(
      env, base::android::ConvertUTF8ToJavaString(env, device_name));
}

void SharedClipboardMessageHandler::SendMessageToDevice(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& j_guid,
    const base::android::JavaParamRef<jstring>& j_message,
    const base::android::JavaParamRef<jobject>& j_runnable) {
  std::string guid = base::android::ConvertJavaStringToUTF8(env, j_guid);
  DCHECK(!guid.empty());

  std::string message = base::android::ConvertJavaStringToUTF8(env, j_message);
  chrome_browser_sharing::SharingMessage sharing_message;
  sharing_message.mutable_shared_clipboard_message()->set_text(
      std::move(message));

  auto callback =
      base::BindOnce(base::android::RunIntCallbackAndroid,
                     base::android::ScopedJavaGlobalRef<jobject>(j_runnable));
  sharing_service_->SendMessageToDevice(
      guid, kSendMessageTimeout, std::move(sharing_message),
      base::BindOnce(
          [](base::OnceCallback<void(int)> callback,
             SharingSendMessageResult result) {
            std::move(callback).Run(static_cast<int>(result));
          },
          std::move(callback)));
}
