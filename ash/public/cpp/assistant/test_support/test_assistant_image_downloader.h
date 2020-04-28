// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASSISTANT_TEST_SUPPORT_TEST_ASSISTANT_IMAGE_DOWNLOADER_H_
#define ASH_PUBLIC_CPP_ASSISTANT_TEST_SUPPORT_TEST_ASSISTANT_IMAGE_DOWNLOADER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/assistant/assistant_image_downloader.h"
#include "base/memory/weak_ptr.h"

namespace ash {

class ASH_PUBLIC_EXPORT TestAssistantImageDownloader
    : public AssistantImageDownloader {
 public:
  TestAssistantImageDownloader();
  TestAssistantImageDownloader(const TestAssistantImageDownloader&) = delete;
  TestAssistantImageDownloader& operator=(TestAssistantImageDownloader&) =
      delete;
  ~TestAssistantImageDownloader() override;

  // AssistantImageDownloader:
  void Download(const AccountId& account_id,
                const GURL& url,
                DownloadCallback callback) override;

 private:
  base::WeakPtrFactory<TestAssistantImageDownloader> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASSISTANT_TEST_SUPPORT_TEST_ASSISTANT_IMAGE_DOWNLOADER_H_
