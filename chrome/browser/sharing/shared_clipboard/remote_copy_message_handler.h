// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_REMOTE_COPY_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_REMOTE_COPY_MESSAGE_HANDLER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "chrome/browser/image_decoder.h"
#include "chrome/browser/sharing/sharing_message_handler.h"

class Profile;

namespace network {
class SimpleURLLoader;
}  // namespace network

// Handles incoming messages for the remote copy feature.
class RemoteCopyMessageHandler : public SharingMessageHandler,
                                 public ImageDecoder::ImageRequest {
 public:
  explicit RemoteCopyMessageHandler(Profile* profile);
  ~RemoteCopyMessageHandler() override;

  // SharingMessageHandler implementation:
  void OnMessage(chrome_browser_sharing::SharingMessage message,
                 DoneCallback done_callback) override;

  // ImageDecoder::ImageRequest implementation:
  void OnImageDecoded(const SkBitmap& decoded_image) override;
  void OnDecodeImageFailed() override;

 private:
  void HandleText(const std::string& text);
  void HandleImage(const std::string& image_url);
  void OnURLLoadComplete(std::unique_ptr<std::string> content);
  void ShowNotification();
  void Finish();

  Profile* profile_ = nullptr;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  std::string device_name_;

  DISALLOW_COPY_AND_ASSIGN(RemoteCopyMessageHandler);
};

#endif  // CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_REMOTE_COPY_MESSAGE_HANDLER_H_
