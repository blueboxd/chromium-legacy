// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/logging/log_buffer.h"

#include <string>

#include "base/strings/utf_string_conversions.h"

namespace autofill {

namespace {

void AppendChildToLastNode(std::vector<base::Value>* buffer,
                           base::Value&& new_child) {
  if (buffer->empty()) {
    buffer->push_back(std::move(new_child));
    return;
  }

  base::Value& parent = buffer->back();

  if (auto* children = parent.FindListKey("children")) {
    children->GetList().push_back(std::move(new_child));
    return;
  }

  base::Value::ListStorage list;
  list.emplace_back(std::move(new_child));
  parent.SetKey("children", base::Value(std::move(list)));
}

}  // namespace

LogBuffer::LogBuffer() = default;
LogBuffer::LogBuffer(LogBuffer&& other) noexcept = default;
LogBuffer::~LogBuffer() = default;

base::Value LogBuffer::RetrieveResult() {
  if (buffer_.empty())
    return base::Value();

  // Close not-yet-closed tags.
  while (buffer_.size() > 1)
    *this << CTag{};

  return std::exchange(buffer_.back(), base::Value());
}

LogBuffer& operator<<(LogBuffer& buf, Tag&& tag) {
  if (!buf.active())
    return buf;

  base::Value::DictStorage storage;
  storage.try_emplace("type", std::make_unique<base::Value>("node"));
  storage.try_emplace("value",
                      std::make_unique<base::Value>(std::move(tag.name)));
  buf.buffer_.emplace_back(std::move(storage));
  return buf;
}

LogBuffer& operator<<(LogBuffer& buf, CTag&& tag) {
  if (!buf.active())
    return buf;
  // Don't close the very first opened tag. It stays and gets returned in the
  // end.
  if (buf.buffer_.size() <= 1)
    return buf;

  base::Value node_to_add = std::move(buf.buffer_.back());
  buf.buffer_.pop_back();

  AppendChildToLastNode(&buf.buffer_, std::move(node_to_add));
  return buf;
}

LogBuffer& operator<<(LogBuffer& buf, Attrib&& attrib) {
  if (!buf.active())
    return buf;

  base::Value& node = buf.buffer_.back();

  if (auto* attributes = node.FindDictKey("attributes")) {
    attributes->SetKey(std::move(attrib.name),
                       base::Value(std::move(attrib.value)));
  } else {
    base::Value::DictStorage dict;
    dict.try_emplace(std::move(attrib.name),
                     std::make_unique<base::Value>(std::move(attrib.value)));
    node.SetKey("attributes", base::Value(std::move(dict)));
  }

  return buf;
}

LogBuffer& operator<<(LogBuffer& buf, Br&& tag) {
  if (!buf.active())
    return buf;
  return buf << Tag{"br"} << CTag{};
}

LogBuffer& operator<<(LogBuffer& buf, base::StringPiece text) {
  if (!buf.active())
    return buf;

  base::Value::DictStorage storage;
  storage.try_emplace("type", std::make_unique<base::Value>("text"));
  // This text is not HTML escaped because the rest of the frame work takes care
  // of that and it must not be escaped twice.
  storage.try_emplace("value", std::make_unique<base::Value>(text));
  base::Value node_to_add(std::move(storage));
  AppendChildToLastNode(&buf.buffer_, std::move(node_to_add));
  return buf;
}

LogBuffer& operator<<(LogBuffer& buf, base::StringPiece16 text) {
  return buf << base::UTF16ToUTF8(text);
}

LogBuffer& operator<<(LogBuffer& buf, LogBuffer&& buffer) {
  if (!buf.active())
    return buf;

  base::Value node_to_add(buffer.RetrieveResult());
  if (node_to_add.is_none())
    return buf;
  AppendChildToLastNode(&buf.buffer_, std::move(node_to_add));
  return buf;
}

LogBuffer& operator<<(LogBuffer& buf, const GURL& url) {
  if (!buf.active())
    return buf;
  if (!url.is_valid())
    return buf << "Invalid URL";
  return buf << url.GetOrigin().spec();
}

}  // namespace autofill
