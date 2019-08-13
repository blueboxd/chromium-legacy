// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_message_handler_android.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/android/chrome_jni_headers/SharedClipboardMessageHandler_jni.h"
#include "chrome/browser/sharing/proto/shared_clipboard_message.pb.h"
#include "chrome/browser/sharing/proto/sharing_message.pb.h"
#include "ui/base/clipboard/clipboard_types.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

SharedClipboardMessageHandler::SharedClipboardMessageHandler() = default;

SharedClipboardMessageHandler::~SharedClipboardMessageHandler() = default;

void SharedClipboardMessageHandler::OnMessage(
    const chrome_browser_sharing::SharingMessage& message) {
  DCHECK(message.has_shared_clipboard_message());

  ui::ScopedClipboardWriter(ui::ClipboardType::kCopyPaste)
      .WriteText(base::UTF8ToUTF16(message.shared_clipboard_message().text()));

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SharedClipboardMessageHandler_showNotification(env);
}
