// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sqlite_proto/predictor_table_base.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "sql/database.h"

namespace predictors {

base::SequencedTaskRunner* PredictorTableBase::GetTaskRunner() {
  return db_task_runner_.get();
}

void PredictorTableBase::ScheduleDBTask(const base::Location& from_here,
                                        DBTask task) {
  GetTaskRunner()->PostTask(
      from_here, base::BindOnce(&PredictorTableBase::ExecuteDBTaskOnDBSequence,
                                this, std::move(task)));
}

void PredictorTableBase::ExecuteDBTaskOnDBSequence(DBTask task) {
  DCHECK(GetTaskRunner()->RunsTasksInCurrentSequence());
  if (CantAccessDatabase())
    return;

  std::move(task).Run(DB());
}

PredictorTableBase::PredictorTableBase(
    scoped_refptr<base::SequencedTaskRunner> db_task_runner)
    : db_task_runner_(std::move(db_task_runner)), db_(nullptr) {}

PredictorTableBase::~PredictorTableBase() = default;

void PredictorTableBase::Initialize(sql::Database* db) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  db_ = db;
  CreateTableIfNonExistent();
}

void PredictorTableBase::SetCancelled() {
  cancelled_.Set();
}

bool PredictorTableBase::IsCancelled() {
  return cancelled_.IsSet();
}

sql::Database* PredictorTableBase::DB() {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  return db_;
}

void PredictorTableBase::ResetDB() {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  db_ = nullptr;
}

bool PredictorTableBase::CantAccessDatabase() {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  return cancelled_.IsSet() || !db_;
}

}  // namespace predictors
