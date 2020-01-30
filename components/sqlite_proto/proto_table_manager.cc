// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sqlite_proto/proto_table_manager.h"

#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/stringprintf.h"
#include "sql/database.h"

namespace sqlite_proto {

namespace {
const char kCreateProtoTableStatementTemplate[] =
    "CREATE TABLE %s ( "
    "key TEXT, "
    "proto BLOB, "
    "PRIMARY KEY(key))";
}  // namespace

ProtoTableManager::ProtoTableManager(
    scoped_refptr<base::SequencedTaskRunner> db_task_runner)
    : TableManager(db_task_runner) {}

ProtoTableManager::~ProtoTableManager() = default;

void ProtoTableManager::InitializeOnDbSequence(
    sql::Database* db,
    base::span<const std::string> table_names) {
  DCHECK(std::set<std::string>(table_names.begin(), table_names.end()).size() ==
         table_names.size());
  DCHECK(!db || db->is_open());
  table_names_.assign(table_names.begin(), table_names.end());
  Initialize(db);  // Superclass method.
}

void ProtoTableManager::CreateTablesIfNonExistent() {
  DCHECK(GetTaskRunner()->RunsTasksInCurrentSequence());

  if (CantAccessDatabase())
    return;

  sql::Database* db = DB();  // Superclass method.
  bool success = db->BeginTransaction();

  for (const std::string& table_name : table_names_) {
    success =
        success &&
        (db->DoesTableExist(table_name.c_str()) ||
         db->Execute(base::StringPrintf(kCreateProtoTableStatementTemplate,
                                        table_name.c_str())
                         .c_str()));
  }

  if (success)
    success = db->CommitTransaction();
  else
    db->RollbackTransaction();

  if (!success)
    ResetDB();  // Resets our non-owning pointer; doesn't mutate the database
                // object.
}
}  // namespace sqlite_proto
