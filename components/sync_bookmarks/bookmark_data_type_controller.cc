// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_data_type_controller.h"

#include <utility>

#include "base/metrics/histogram.h"
#include "components/sync/driver/model_associator.h"
#include "components/sync/driver/sync_api_component_factory.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/model/change_processor.h"

using bookmarks::BookmarkModel;

namespace sync_bookmarks {

BookmarkDataTypeController::BookmarkDataTypeController(
    const base::RepeatingClosure& dump_stack,
    syncer::SyncService* sync_service,
    bookmarks::BookmarkModel* bookmark_model,
    history::HistoryService* history_service,
    syncer::SyncApiComponentFactory* component_factory)
    : syncer::FrontendDataTypeController(syncer::BOOKMARKS,
                                         dump_stack,
                                         sync_service),
      bookmark_model_(bookmark_model),
      history_service_(history_service),
      component_factory_(component_factory) {}

BookmarkDataTypeController::~BookmarkDataTypeController() {}

bool BookmarkDataTypeController::StartModels() {
  DCHECK(CalledOnValidThread());
  if (!DependentsLoaded()) {
    bookmark_model_observer_.Add(bookmark_model_);
    history_service_observer_.Add(history_service_);
    return false;
  }
  return true;
}

void BookmarkDataTypeController::CleanUpState() {
  DCHECK(CalledOnValidThread());
  history_service_observer_.RemoveAll();
  bookmark_model_observer_.RemoveAll();
}

void BookmarkDataTypeController::CreateSyncComponents() {
  DCHECK(CalledOnValidThread());
  syncer::SyncApiComponentFactory::SyncComponents sync_components =
      component_factory_->CreateBookmarkSyncComponents(
          CreateErrorHandler(), sync_service()->GetUserShare());
  set_model_associator(std::move(sync_components.model_associator));
  set_change_processor(std::move(sync_components.change_processor));
}

void BookmarkDataTypeController::BookmarkModelChanged() {
}

void BookmarkDataTypeController::BookmarkModelLoaded(BookmarkModel* model,
                                                     bool ids_reassigned) {
  DCHECK(CalledOnValidThread());
  DCHECK(model->loaded());
  bookmark_model_observer_.RemoveAll();

  if (!DependentsLoaded())
    return;

  history_service_observer_.RemoveAll();
  OnModelLoaded();
}

void BookmarkDataTypeController::BookmarkModelBeingDeleted(
    BookmarkModel* model) {
  CleanUpState();
}

// Check that both the bookmark model and the history service (for favicons)
// are loaded.
bool BookmarkDataTypeController::DependentsLoaded() {
  DCHECK(CalledOnValidThread());
  if (!bookmark_model_ || !bookmark_model_->loaded())
    return false;

  if (!history_service_ || !history_service_->BackendLoaded())
    return false;

  // All necessary services are loaded.
  return true;
}

void BookmarkDataTypeController::OnHistoryServiceLoaded(
    history::HistoryService* service) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(state_, MODEL_STARTING);
  history_service_observer_.RemoveAll();

  if (!DependentsLoaded())
    return;

  bookmark_model_observer_.RemoveAll();
  OnModelLoaded();
}

void BookmarkDataTypeController::HistoryServiceBeingDeleted(
    history::HistoryService* history_service) {
  CleanUpState();
}

}  // namespace sync_bookmarks
