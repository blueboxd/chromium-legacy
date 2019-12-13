// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webdata/common/web_data_service_base.h"

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread.h"
#include "components/webdata/common/web_database_service.h"

////////////////////////////////////////////////////////////////////////////////
//
// WebDataServiceBase implementation.
//
////////////////////////////////////////////////////////////////////////////////

using base::Bind;
using base::Time;

WebDataServiceBase::WebDataServiceBase(
    scoped_refptr<WebDatabaseService> wdbs,
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner)
    : base::RefCountedDeleteOnSequence<WebDataServiceBase>(ui_task_runner),
      wdbs_(wdbs) {}

void WebDataServiceBase::ShutdownOnUISequence() {}

void WebDataServiceBase::Init(ProfileErrorCallback callback) {
  DCHECK(wdbs_);
  wdbs_->RegisterDBErrorCallback(std::move(callback));
  wdbs_->LoadDatabase();
}

void WebDataServiceBase::ShutdownDatabase() {
  if (!wdbs_)
    return;
  wdbs_->ShutdownDatabase();
}

void WebDataServiceBase::CancelRequest(Handle h) {
  if (!wdbs_)
    return;
  wdbs_->CancelRequest(h);
}

bool WebDataServiceBase::IsDatabaseLoaded() {
  if (!wdbs_)
    return false;
  return wdbs_->db_loaded();
}

void WebDataServiceBase::RegisterDBLoadedCallback(DBLoadedCallback callback) {
  if (!wdbs_)
    return;
  wdbs_->RegisterDBLoadedCallback(std::move(callback));
}

WebDatabase* WebDataServiceBase::GetDatabase() {
  if (!wdbs_)
    return nullptr;
  return wdbs_->GetDatabaseOnDB();
}

WebDataServiceBase::~WebDataServiceBase() {
}
