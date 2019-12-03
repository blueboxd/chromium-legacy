// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PROVIDER_BOOKMARK_MODEL_TASK_H_
#define CHROME_BROWSER_ANDROID_PROVIDER_BOOKMARK_MODEL_TASK_H_

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"

namespace bookmarks {
class BookmarkModel;
class ModelLoader;
}  // namespace bookmarks

// Base class for synchronous tasks that involve the bookmark model.
// Ensures the model has been loaded before accessing it.
// Must not be created from the UI thread.
class BookmarkModelTask {
 public:
  BookmarkModelTask(base::WeakPtr<bookmarks::BookmarkModel> model,
                    scoped_refptr<bookmarks::ModelLoader> model_loader);
  ~BookmarkModelTask();

  base::WeakPtr<bookmarks::BookmarkModel> model() const;

 private:
  base::WeakPtr<bookmarks::BookmarkModel> model_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkModelTask);
};

#endif  // CHROME_BROWSER_ANDROID_PROVIDER_BOOKMARK_MODEL_TASK_H_
