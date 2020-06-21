// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/test_download_shelf.h"

#include "content/public/browser/download_manager.h"

TestDownloadShelf::TestDownloadShelf(Profile* profile)
    : DownloadShelf(nullptr, profile),
      is_showing_(false),
      did_add_download_(false) {}

TestDownloadShelf::~TestDownloadShelf() = default;

bool TestDownloadShelf::IsShowing() const {
  return is_showing_;
}

bool TestDownloadShelf::IsClosing() const {
  return false;
}

void TestDownloadShelf::DoShowDownload(
    DownloadUIModel::DownloadUIModelPtr download) {
  did_add_download_ = true;
}

void TestDownloadShelf::DoOpen() {
  is_showing_ = true;
}

void TestDownloadShelf::DoClose() {
  is_showing_ = false;
}

void TestDownloadShelf::DoHide() {
  is_showing_ = false;
}

void TestDownloadShelf::DoUnhide() {
  is_showing_ = true;
}

base::TimeDelta TestDownloadShelf::GetTransientDownloadShowDelay() const {
  return base::TimeDelta();
}
