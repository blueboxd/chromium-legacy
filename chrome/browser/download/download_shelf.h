// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SHELF_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SHELF_H_

#include <stdint.h>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_ui_model.h"

class Browser;

namespace offline_items_collection {
struct ContentId;
}  // namespace offline_items_collection

// This is an abstract base class for platform specific download shelf
// implementations.
class DownloadShelf {
 public:
  DownloadShelf(Browser* browser, Profile* profile);
  virtual ~DownloadShelf();

  // The browser view needs to know when we are going away to properly return
  // the resize corner size to WebKit so that we don't draw on top of it.
  // This returns the showing state of our animation which is set to true at
  // the beginning Show and false at the beginning of a Hide.
  virtual bool IsShowing() const = 0;

  // Returns whether the download shelf is showing the close animation.
  virtual bool IsClosing() const = 0;

  // A new download has started. Add it to our shelf and show the download
  // started animation.
  //
  // Some downloads are removed from the shelf on completion (See
  // DownloadItemModel::ShouldRemoveFromShelfWhenComplete()). These transient
  // downloads are added to the shelf after a delay. If the download completes
  // before the delay duration, it will not be added to the shelf at all.
  void AddDownload(DownloadUIModel::DownloadUIModelPtr download);

  // Opens the shelf.
  void Open();

  // Closes the shelf.
  void Close();

  // Hides the shelf. This closes the shelf if it is currently showing.
  void Hide();

  // Unhides the shelf. This will cause the shelf to be opened if it was open
  // when it was hidden, or was shown while it was hidden.
  void Unhide();

  Browser* browser() { return browser_; }

  // Returns whether the download shelf is hidden.
  bool is_hidden() { return is_hidden_; }

 protected:
  virtual void DoShowDownload(DownloadUIModel::DownloadUIModelPtr download) = 0;
  virtual void DoOpen() = 0;
  virtual void DoClose() = 0;
  virtual void DoHide() = 0;
  virtual void DoUnhide() = 0;

  // Time delay to wait before adding a transient download to the shelf.
  // Protected virtual for testing.
  virtual base::TimeDelta GetTransientDownloadShowDelay() const;

  Profile* profile() { return profile_; }

 private:
  // Show the download on the shelf immediately. Also displayes the download
  // started animation if necessary.
  void ShowDownload(DownloadUIModel::DownloadUIModelPtr download);

  // Similar to ShowDownload() but refers to the download using an ID.
  void ShowDownloadById(const offline_items_collection::ContentId& id);

  Browser* const browser_;
  Profile* const profile_;
  bool should_show_on_unhide_;
  bool is_hidden_;
  base::WeakPtrFactory<DownloadShelf> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SHELF_H_
