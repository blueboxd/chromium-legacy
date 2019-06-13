// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/status_icons/dbus_types.h"

#include "dbus/message.h"
#include "dbus/object_path.h"

DbusType::~DbusType() = default;

DbusBoolean::DbusBoolean(bool value) : value_(value) {}
DbusBoolean::DbusBoolean(DbusBoolean&& other) = default;
DbusBoolean::~DbusBoolean() = default;

void DbusBoolean::Write(dbus::MessageWriter* writer) const {
  writer->AppendBool(value_);
}

// static
std::string DbusBoolean::GetSignature() {
  return "b";
}

DbusInt32::DbusInt32(int32_t value) : value_(value) {}
DbusInt32::DbusInt32(DbusInt32&& other) = default;
DbusInt32::~DbusInt32() = default;

void DbusInt32::Write(dbus::MessageWriter* writer) const {
  writer->AppendInt32(value_);
}

// static
std::string DbusInt32::GetSignature() {
  return "i";
}

DbusString::DbusString(const std::string& value) : value_(value) {}
DbusString::DbusString(DbusString&& other) = default;
DbusString::~DbusString() = default;

void DbusString::Write(dbus::MessageWriter* writer) const {
  writer->AppendString(value_);
}

// static
std::string DbusString::GetSignature() {
  return "s";
}

DbusObjectPath::DbusObjectPath(const dbus::ObjectPath& value) : value_(value) {}
DbusObjectPath::DbusObjectPath(DbusObjectPath&& other) = default;
DbusObjectPath::~DbusObjectPath() = default;

void DbusObjectPath::Write(dbus::MessageWriter* writer) const {
  writer->AppendObjectPath(value_);
}

// static
std::string DbusObjectPath::GetSignature() {
  return "o";
}

DbusVariant::DbusVariant() = default;
DbusVariant::DbusVariant(std::unique_ptr<DbusType> value)
    : value_(std::move(value)) {}
DbusVariant::DbusVariant(DbusVariant&& other) = default;
DbusVariant::~DbusVariant() = default;
DbusVariant& DbusVariant::operator=(DbusVariant&& other) = default;

void DbusVariant::Write(dbus::MessageWriter* writer) const {
  dbus::MessageWriter variant_writer(nullptr);
  writer->OpenVariant(value_->GetSignatureDynamic(), &variant_writer);
  value_->Write(&variant_writer);
  writer->CloseContainer(&variant_writer);
}

// static
std::string DbusVariant::GetSignature() {
  return "v";
}

DbusByteArray::DbusByteArray() = default;
DbusByteArray::DbusByteArray(std::vector<uint8_t>&& value)
    : value_(std::move(value)) {}
DbusByteArray::DbusByteArray(DbusByteArray&& other) = default;
DbusByteArray::~DbusByteArray() = default;

void DbusByteArray::Write(dbus::MessageWriter* writer) const {
  writer->AppendArrayOfBytes(value_.data(), value_.size());
}

// static
std::string DbusByteArray::GetSignature() {
  return "ay";  // lmao
}
