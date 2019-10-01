// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_SERVICE_H_
#define COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "components/dom_distiller/core/article_entry.h"
#include "components/dom_distiller/core/distilled_page_prefs.h"
#include "components/dom_distiller/core/distiller_page.h"

class GURL;

namespace dom_distiller {

class DistilledContentStore;
class DistillerFactory;
class DistillerPageFactory;
class DomDistillerStoreInterface;
class TaskTracker;
class ViewerHandle;
class ViewRequestDelegate;

// Service for interacting with the Dom Distiller.
// Construction, destruction, and usage of this service must happen on the same
// thread. Callbacks will be called on that same thread.
class DomDistillerServiceInterface {
 public:
  typedef base::Callback<void(bool)> ArticleAvailableCallback;
  virtual ~DomDistillerServiceInterface() {}

  // Returns whether an article stored has the given entry id.
  virtual bool HasEntry(const std::string& entry_id) = 0;

  // Returns the source URL given an entry ID. If the entry ID article has
  // multiple pages, this will return the URL of the first page. Returns an
  // empty string if there is no entry associated with the given entry ID.
  virtual std::string GetUrlForEntry(const std::string& entry_id) = 0;

  // Request to view an article by entry id. Returns a null pointer if no entry
  // with |entry_id| exists. The ViewerHandle should be destroyed before the
  // ViewRequestDelegate. The request will be cancelled when the handle is
  // destroyed (or when this service is destroyed), which also ensures that
  // the |delegate| is not called after that.
  // Use CreateDefaultDistillerPage() to create a default |distiller_page|.
  // The provided |distiller_page| is only used if there is not already a
  // distillation task in progress for the given |entry_id|.
  virtual std::unique_ptr<ViewerHandle> ViewEntry(
      ViewRequestDelegate* delegate,
      std::unique_ptr<DistillerPage> distiller_page,
      const std::string& entry_id) = 0;

  // Request to view an article by url.
  // Use CreateDefaultDistillerPage() to create a default |distiller_page|.
  // The provided |distiller_page| is only used if there is not already a
  // distillation task in progress for the given |url|.
  virtual std::unique_ptr<ViewerHandle> ViewUrl(
      ViewRequestDelegate* delegate,
      std::unique_ptr<DistillerPage> distiller_page,
      const GURL& url) = 0;

  // Creates a default DistillerPage.
  virtual std::unique_ptr<DistillerPage> CreateDefaultDistillerPage(
      const gfx::Size& render_view_size) = 0;
  virtual std::unique_ptr<DistillerPage> CreateDefaultDistillerPageWithHandle(
      std::unique_ptr<SourcePageHandle> handle) = 0;

  // Returns the DistilledPagePrefs owned by the instance of
  // DomDistillerService.
  virtual DistilledPagePrefs* GetDistilledPagePrefs() = 0;

 protected:
  DomDistillerServiceInterface() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DomDistillerServiceInterface);
};

// Provide a view of the article list and ways of interacting with it.
class DomDistillerService : public DomDistillerServiceInterface {
 public:
  DomDistillerService(
      std::unique_ptr<DomDistillerStoreInterface> store,
      std::unique_ptr<DistillerFactory> distiller_factory,
      std::unique_ptr<DistillerPageFactory> distiller_page_factory,
      std::unique_ptr<DistilledPagePrefs> distilled_page_prefs);
  ~DomDistillerService() override;

  // DomDistillerServiceInterface implementation.
  bool HasEntry(const std::string& entry_id) override;
  std::string GetUrlForEntry(const std::string& entry_id) override;
  std::unique_ptr<ViewerHandle> ViewEntry(
      ViewRequestDelegate* delegate,
      std::unique_ptr<DistillerPage> distiller_page,
      const std::string& entry_id) override;
  std::unique_ptr<ViewerHandle> ViewUrl(
      ViewRequestDelegate* delegate,
      std::unique_ptr<DistillerPage> distiller_page,
      const GURL& url) override;
  std::unique_ptr<DistillerPage> CreateDefaultDistillerPage(
      const gfx::Size& render_view_size) override;
  std::unique_ptr<DistillerPage> CreateDefaultDistillerPageWithHandle(
      std::unique_ptr<SourcePageHandle> handle) override;
  DistilledPagePrefs* GetDistilledPagePrefs() override;

 private:
  void CancelTask(TaskTracker* task);

  TaskTracker* CreateTaskTracker(const ArticleEntry& entry);

  TaskTracker* GetTaskTrackerForEntry(const ArticleEntry& entry) const;
  TaskTracker* GetTaskTrackerForUrl(const GURL& url) const;

  // Gets the task tracker for the given |url| or |entry|. If no appropriate
  // tracker exists, this will create one and put it in the |TaskTracker|
  // parameter passed into this function, initialize it, and add it to
  // |tasks_|. If a |TaskTracker| needed to be created, these functions will
  // return true.
  bool GetOrCreateTaskTrackerForUrl(const GURL& url,
                                    TaskTracker** task_tracker);
  bool GetOrCreateTaskTrackerForEntry(const ArticleEntry& entry,
                                      TaskTracker** task_tracker);

  std::unique_ptr<DomDistillerStoreInterface> store_;
  std::unique_ptr<DistilledContentStore> content_store_;
  std::unique_ptr<DistillerFactory> distiller_factory_;
  std::unique_ptr<DistillerPageFactory> distiller_page_factory_;
  std::unique_ptr<DistilledPagePrefs> distilled_page_prefs_;

  typedef std::vector<std::unique_ptr<TaskTracker>> TaskList;
  TaskList tasks_;

  DISALLOW_COPY_AND_ASSIGN(DomDistillerService);
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_SERVICE_H_
