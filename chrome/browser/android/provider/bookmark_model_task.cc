// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/provider/bookmark_model_task.h"

#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/model_loader.h"
#include "content/public/browser/browser_thread.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using content::BrowserThread;

BookmarkModelTask::BookmarkModelTask(
    base::WeakPtr<bookmarks::BookmarkModel> model,
    scoped_refptr<bookmarks::ModelLoader> model_loader)
    : model_(std::move(model)) {
  // Ensure the initialization of the native bookmark model.
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  model_loader->BlockTillLoaded();
}

BookmarkModelTask::~BookmarkModelTask() = default;

base::WeakPtr<BookmarkModel> BookmarkModelTask::model() const {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  return model_;
}
