// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/assistant/test_support/test_assistant_image_downloader.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {

TestAssistantImageDownloader::TestAssistantImageDownloader() = default;

TestAssistantImageDownloader::~TestAssistantImageDownloader() = default;

void TestAssistantImageDownloader::Download(const AccountId& account_id,
                                            const GURL& url,
                                            DownloadCallback callback) {
  gfx::ImageSkia image = gfx::test::CreateImageSkia(
      /*width=*/10, /*height=*/10);

  // Pretend to respond asynchronously.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), image));
}

}  // namespace ash
