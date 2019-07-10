// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DBUS_MENU_TYPES_H_
#define COMPONENTS_DBUS_MENU_TYPES_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include "base/component_export.h"
#include "base/memory/ref_counted_memory.h"
#include "dbus/message.h"

namespace detail {

template <std::size_t i,
          typename... Ts,
          std::enable_if_t<i == sizeof...(Ts), int> = 0>
void WriteDbusTypeTuple(const std::tuple<Ts...>&, dbus::MessageWriter*) {}

template <std::size_t i,
          typename... Ts,
          std::enable_if_t<(i < sizeof...(Ts)), int> = 0>
void WriteDbusTypeTuple(const std::tuple<Ts...>& ts,
                        dbus::MessageWriter* writer) {
  std::get<i>(ts).Write(writer);
  WriteDbusTypeTuple<i + 1, Ts...>(ts, writer);
}

template <typename... Ts>
void WriteDbusTypeTuple(const std::tuple<Ts...>& ts,
                        dbus::MessageWriter* writer) {
  WriteDbusTypeTuple<0, Ts...>(ts, writer);
}

template <std::size_t i,
          typename... Ts,
          std::enable_if_t<i == sizeof...(Ts), int> = 0>
std::string GetDbusTypeTupleSignature() {
  return std::string();
}

template <std::size_t i,
          typename... Ts,
          std::enable_if_t<(i < sizeof...(Ts)), int> = 0>
std::string GetDbusTypeTupleSignature() {
  return std::tuple_element_t<i, std::tuple<Ts...>>::GetSignature() +
         GetDbusTypeTupleSignature<i + 1, Ts...>();
}

template <typename... Ts>
std::string GetDbusTypeTupleSignature() {
  return GetDbusTypeTupleSignature<0, Ts...>();
}

}  // namespace detail

class COMPONENT_EXPORT(DBUS) DbusType {
 public:
  virtual ~DbusType();

  // Serializes this object to |writer|.
  virtual void Write(dbus::MessageWriter* writer) const = 0;

  // Both a virtual and a static version of GetSignature() are necessary.
  // The virtual version is needed by DbusVariant which needs to know the
  // signature at runtime since you could eg. have an array of variants of
  // heterogeneous types.  The static version is needed by DbusArray: if the
  // array is empty, then there would be no DbusType instance to get the
  // signature from.
  virtual std::string GetSignatureDynamic() const = 0;
};

template <typename T>
class DbusTypeImpl : public DbusType {
 public:
  ~DbusTypeImpl() override {}

  // DbusType:
  std::string GetSignatureDynamic() const override { return T::GetSignature(); }
};

class COMPONENT_EXPORT(DBUS) DbusBoolean : public DbusTypeImpl<DbusBoolean> {
 public:
  explicit DbusBoolean(bool value);
  DbusBoolean(DbusBoolean&& other);
  ~DbusBoolean() override;

  // DbusType:
  void Write(dbus::MessageWriter* writer) const override;

  static std::string GetSignature();

 private:
  bool value_;
};

class COMPONENT_EXPORT(DBUS) DbusInt32 : public DbusTypeImpl<DbusInt32> {
 public:
  explicit DbusInt32(int32_t value);
  DbusInt32(DbusInt32&& other);
  ~DbusInt32() override;

  // DbusType:
  void Write(dbus::MessageWriter* writer) const override;

  static std::string GetSignature();

 private:
  int32_t value_;
};

class COMPONENT_EXPORT(DBUS) DbusUint32 : public DbusTypeImpl<DbusUint32> {
 public:
  explicit DbusUint32(uint32_t value);
  DbusUint32(DbusUint32&& other);
  ~DbusUint32() override;

  // DbusType:
  void Write(dbus::MessageWriter* writer) const override;

  static std::string GetSignature();

 private:
  uint32_t value_;
};

class COMPONENT_EXPORT(DBUS) DbusString : public DbusTypeImpl<DbusString> {
 public:
  explicit DbusString(const std::string& value);
  DbusString(DbusString&& other);
  ~DbusString() override;

  // DbusType:
  void Write(dbus::MessageWriter* writer) const override;

  static std::string GetSignature();

 private:
  std::string value_;
};

class COMPONENT_EXPORT(DBUS) DbusObjectPath
    : public DbusTypeImpl<DbusObjectPath> {
 public:
  explicit DbusObjectPath(const dbus::ObjectPath& value);
  DbusObjectPath(DbusObjectPath&& other);
  ~DbusObjectPath() override;

  // DbusType:
  void Write(dbus::MessageWriter* writer) const override;

  static std::string GetSignature();

 private:
  dbus::ObjectPath value_;
};

class COMPONENT_EXPORT(DBUS) DbusVariant : public DbusTypeImpl<DbusVariant> {
 public:
  DbusVariant();
  explicit DbusVariant(std::unique_ptr<DbusType> value);
  DbusVariant(DbusVariant&& other);
  ~DbusVariant() override;

  DbusVariant& operator=(DbusVariant&& other);

  operator bool() const;

  // DbusType:
  void Write(dbus::MessageWriter* writer) const override;

  static std::string GetSignature();

 private:
  std::unique_ptr<DbusType> value_;
};

template <typename T>
DbusVariant MakeDbusVariant(T t) {
  return DbusVariant(std::make_unique<T>(std::move(t)));
}

template <typename T>
class COMPONENT_EXPORT(DBUS) DbusArray : public DbusTypeImpl<DbusArray<T>> {
 public:
  explicit DbusArray(std::vector<T>&& value) : value_(std::move(value)) {}
  DbusArray(DbusArray<T>&& other) = default;
  ~DbusArray() override = default;

  template <typename... Ts>
  explicit DbusArray(Ts&&... ts) {
    value_.reserve(sizeof...(ts));
    int dummy[] = {0, (value_.push_back(std::move(ts)), 0)...};
    (void)dummy;
  }

  // DbusType:
  void Write(dbus::MessageWriter* writer) const override {
    dbus::MessageWriter array_writer(nullptr);
    writer->OpenArray(T::GetSignature(), &array_writer);
    for (const T& t : value_)
      t.Write(&array_writer);
    writer->CloseContainer(&array_writer);
  }

  static std::string GetSignature() {
    return std::string("a") + T::GetSignature();
  }

 private:
  std::vector<T> value_;
};

template <typename... Ts>
auto MakeDbusArray(Ts&&... ts) {
  return DbusArray<std::common_type_t<Ts...>>{std::move(ts)...};
}

// (If DbusByte was defined) this is the same as DbusArray<DbusByte>.  This
// class avoids having to create a bunch of heavy virtual objects just to wrap
// individual bytes.
class COMPONENT_EXPORT(DBUS) DbusByteArray
    : public DbusTypeImpl<DbusByteArray> {
 public:
  DbusByteArray();
  explicit DbusByteArray(scoped_refptr<base::RefCountedMemory> value);
  DbusByteArray(DbusByteArray&& other);
  ~DbusByteArray() override;

  // DbusType:
  void Write(dbus::MessageWriter* writer) const override;

  static std::string GetSignature();

 private:
  scoped_refptr<base::RefCountedMemory> value_;
};

template <typename... Ts>
class COMPONENT_EXPORT(DBUS) DbusStruct
    : public DbusTypeImpl<DbusStruct<Ts...>> {
 public:
  explicit DbusStruct(Ts&&... ts) : value_(std::move(ts)...) {}
  DbusStruct(DbusStruct<Ts...>&& other) = default;
  ~DbusStruct() override = default;

  // DbusType:
  void Write(dbus::MessageWriter* writer) const override {
    dbus::MessageWriter struct_writer(nullptr);
    writer->OpenStruct(&struct_writer);
    detail::WriteDbusTypeTuple(value_, &struct_writer);
    writer->CloseContainer(&struct_writer);
  }

  static std::string GetSignature() {
    return "(" + detail::GetDbusTypeTupleSignature<Ts...>() + ")";
  }

 private:
  std::tuple<Ts...> value_;
};

template <typename... Ts>
auto MakeDbusStruct(Ts&&... ts) {
  return DbusStruct<Ts...>{std::move(ts)...};
}

template <typename K, typename V>
class COMPONENT_EXPORT(DBUS) DbusDictEntry
    : public DbusTypeImpl<DbusDictEntry<K, V>> {
 public:
  DbusDictEntry(K&& k, V&& v) : k_(std::move(k)), v_(std::move(v)) {}
  DbusDictEntry(DbusDictEntry<K, V>&& other) = default;
  ~DbusDictEntry() override = default;

  // DbusType:
  void Write(dbus::MessageWriter* writer) const override {
    dbus::MessageWriter dict_entry_writer(nullptr);
    writer->OpenDictEntry(&dict_entry_writer);
    k_.Write(&dict_entry_writer);
    v_.Write(&dict_entry_writer);
    writer->CloseContainer(&dict_entry_writer);
  }

  static std::string GetSignature() {
    return "{" + K::GetSignature() + V::GetSignature() + "}";
  }

 private:
  K k_;
  V v_;
};

template <typename K, typename V>
auto MakeDbusDictEntry(K&& k, V&& v) {
  return DbusDictEntry<K, V>(std::move(k), std::move(v));
}

#endif  // COMPONENTS_DBUS_MENU_TYPES_H_
