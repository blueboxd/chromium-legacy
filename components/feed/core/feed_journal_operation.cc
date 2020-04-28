// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/feed_journal_operation.h"

#include <utility>

#include "base/check_op.h"

namespace feed {

// static
JournalOperation JournalOperation::CreateAppendOperation(std::string value) {
  return JournalOperation(JOURNAL_APPEND, std::move(value), std::string());
}

// static
JournalOperation JournalOperation::CreateCopyOperation(
    std::string to_journal_name) {
  return JournalOperation(JOURNAL_COPY, std::string(),
                          std::move(to_journal_name));
}

// static
JournalOperation JournalOperation::CreateDeleteOperation() {
  return JournalOperation(JOURNAL_DELETE, std::string(), std::string());
}

JournalOperation::JournalOperation(JournalOperation&& operation) = default;

JournalOperation::Type JournalOperation::type() {
  return type_;
}

const std::string& JournalOperation::value() {
  DCHECK_EQ(type_, JOURNAL_APPEND);
  return value_;
}

const std::string& JournalOperation::to_journal_name() {
  DCHECK_EQ(type_, JOURNAL_COPY);
  return to_journal_name_;
}

JournalOperation::JournalOperation(Type type,
                                   std::string value,
                                   std::string to_journal_name)
    : type_(type),
      value_(std::move(value)),
      to_journal_name_(std::move(to_journal_name)) {}

}  // namespace feed
