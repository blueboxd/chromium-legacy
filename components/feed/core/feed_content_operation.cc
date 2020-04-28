// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/feed_content_operation.h"

#include <utility>

#include "base/check_op.h"

namespace feed {

// static
ContentOperation ContentOperation::CreateDeleteOperation(std::string key) {
  return ContentOperation(CONTENT_DELETE, std::move(key), std::string(),
                          std::string());
}

// static
ContentOperation ContentOperation::CreateDeleteAllOperation() {
  return ContentOperation(CONTENT_DELETE_ALL, std::string(), std::string(),
                          std::string());
}

// static
ContentOperation ContentOperation::CreateDeleteByPrefixOperation(
    std::string prefix) {
  return ContentOperation(CONTENT_DELETE_BY_PREFIX, std::string(),
                          std::string(), std::move(prefix));
}

// static
ContentOperation ContentOperation::CreateUpsertOperation(std::string key,
                                                         std::string value) {
  return ContentOperation(CONTENT_UPSERT, std::move(key), std::move(value),
                          std::string());
}

ContentOperation::ContentOperation(ContentOperation&& operation) = default;

ContentOperation::Type ContentOperation::type() {
  return type_;
}

const std::string& ContentOperation::key() {
  DCHECK(type_ == CONTENT_UPSERT || type_ == CONTENT_DELETE);
  return key_;
}

const std::string& ContentOperation::value() {
  DCHECK_EQ(type_, CONTENT_UPSERT);
  return value_;
}

const std::string& ContentOperation::prefix() {
  DCHECK_EQ(type_, CONTENT_DELETE_BY_PREFIX);
  return prefix_;
}

ContentOperation::ContentOperation(Type type,
                                   std::string key,
                                   std::string value,
                                   std::string prefix)
    : type_(type),
      key_(std::move(key)),
      value_(std::move(value)),
      prefix_(std::move(prefix)) {}

}  // namespace feed
