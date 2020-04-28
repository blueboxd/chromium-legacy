// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/font_access/font_metadata.h"

#include "base/big_endian.h"
#include "base/sys_byteorder.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/modules/font_access/font_table_map.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/skia/include/core/SkTypes.h"

namespace {

enum FontAccessStatus {
  kOk,
  // An invalid argument was passed to a method.
  kInvalidArgument,
};

struct FontAccessError {
  FontAccessStatus status;
  String message;

  bool IsOk() { return status == FontAccessStatus::kOk; }

  static FontAccessError CreateOk() {
    return FontAccessError{FontAccessStatus::kOk, ""};
  }

  static FontAccessError CreateInvalidArgument(String message) {
    return FontAccessError{FontAccessStatus::kInvalidArgument, message};
  }
};

bool isValidTableNameCharacter(unsigned int name_char) {
  // According to the OpenType specifications, each byte in a Tag has a value
  // between 0x20 and 0x7E.
  // https://docs.microsoft.com/en-us/typography/opentype/spec/otff#data-types
  return (name_char >= 0x20 && name_char <= 0x7E);
}

bool isValidTableName(const String& table_name) {
  // According to the OpenType specifications, a Tag may be interpreted as a
  // an array with 4 elements. The String representation will be 4 characters,
  // each character representing one byte of a Tag.
  if (table_name.length() != 4) {
    return false;
  }

  for (unsigned i = 0; i < 4; ++i) {
    if (!isValidTableNameCharacter(table_name[i]))
      return false;
  }
  return true;
}

String convertTagToString(uint32_t tag) {
#if defined(ARCH_CPU_LITTLE_ENDIAN)
  // Tag assumed to be in Big Endian byte ordering.
  // Swap byte ordering if on Little Endian architecture.
  tag = base::ByteSwap(tag);
#endif
  char buf[5];
  std::memcpy(&buf, &tag, 4);
  buf[4] = '\0';
  return String(&buf[0], size_t{4});
}

std::pair<FontAccessError, uint32_t> convertStringToTag(
    const String& table_name) {
  if (!isValidTableName(table_name)) {
    return std::make_pair(
        FontAccessError::CreateInvalidArgument(String::Format(
            "Invalid table name: %s", table_name.Latin1().c_str())),
        0u);
  }

  // Tags in OpenType are always in Big Endian order.
  // https://docs.microsoft.com/en-us/typography/opentype/spec/otff#data-types
  uint32_t tag;

  // Since we know that table_name is "valid", i.e. all characters are within
  // the ASCII range, we can safely assume the following works, whether or not
  // each character is 8 or 16 bit encoded.
  base::ReadBigEndian(table_name.Ascii().c_str(), &tag);

  return std::make_pair(FontAccessError::CreateOk(), tag);
}

std::pair<FontAccessError, Vector<uint32_t>> convertTableNamesToTags(
    const Vector<String>& table_names) {
  Vector<uint32_t> output;
  for (auto tname : table_names) {
    auto tag_result = convertStringToTag(tname);
    if (!tag_result.first.IsOk()) {
      return std::make_pair(tag_result.first, Vector<uint32_t>());
    }
    output.push_back(tag_result.second);
  }
  return std::make_pair(FontAccessError::CreateOk(), std::move(output));
}

}  // namespace

namespace blink {

FontMetadata::FontMetadata(const FontEnumerationEntry& entry)
    : postscriptName_(entry.postscript_name),
      fullName_(entry.full_name),
      family_(entry.family) {}

FontMetadata* FontMetadata::Create(const FontEnumerationEntry& entry) {
  return MakeGarbageCollected<FontMetadata>(entry);
}

ScriptPromise FontMetadata::getTables(ScriptState* script_state) {
  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  Vector<String> tables;

  Thread::Current()->GetTaskRunner()->PostTask(
      FROM_HERE,
      WTF::Bind(&FontMetadata::getTablesImpl, WrapPersistent(resolver),
                postscriptName_, std::move(tables)));

  return promise;
}

ScriptPromise FontMetadata::getTables(ScriptState* script_state,
                                      const Vector<String>& tables) {
  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  Thread::Current()->GetTaskRunner()->PostTask(
      FROM_HERE, WTF::Bind(&FontMetadata::getTablesImpl,
                           WrapPersistent(resolver), postscriptName_, tables));

  return promise;
}

// static
void FontMetadata::getTablesImpl(ScriptPromiseResolver* resolver,
                                 const String& postscriptName,
                                 const Vector<String>& tableNames) {
  if (!resolver->GetScriptState()->ContextIsValid())
    return;

  FontDescription description;
  scoped_refptr<SimpleFontData> font_data =
      FontCache::GetFontCache()->GetFontData(description,
                                             AtomicString(postscriptName));
  if (!font_data) {
    auto message = String::Format("The font %s could not be accessed.",
                                  postscriptName.Latin1().c_str());
    ScriptState::Scope scope(resolver->GetScriptState());
    resolver->Reject(V8ThrowException::CreateTypeError(
        resolver->GetScriptState()->GetIsolate(), message));
    return;
  }

  const SkTypeface* typeface = font_data->PlatformData().Typeface();
  Vector<SkFontTableTag> tags;
  if (tableNames.IsEmpty()) {
    // No tables supplied. Fetch all.
    const int num_tables = typeface->countTables();

    SkFontTableTag sk_tags[num_tables];
    const unsigned int ret_val = typeface->getTableTags(sk_tags);
    if (ret_val == 0) {
      auto message = String::Format("Error enumerating tables for font %s.",
                                    postscriptName.Latin1().c_str());
      ScriptState::Scope scope(resolver->GetScriptState());
      resolver->Reject(V8ThrowException::CreateTypeError(
          resolver->GetScriptState()->GetIsolate(), message));
      return;
    }
    tags.Append(sk_tags, num_tables);
    DCHECK(ret_val == tags.size());
  } else {
    // Table names supplied. Validate and convert for retrieval.
    auto result = convertTableNamesToTags(tableNames);
    if (!result.first.IsOk()) {
      ScriptState::Scope scope(resolver->GetScriptState());
      resolver->Reject(V8ThrowException::CreateTypeError(
          resolver->GetScriptState()->GetIsolate(), result.first.message));
      return;
    }
    tags = result.second;
  }

  auto* output = MakeGarbageCollected<HeapHashMap<String, Member<Blob>>>();

  for (auto tag : tags) {
    wtf_size_t table_size = SafeCast<wtf_size_t>(typeface->getTableSize(tag));
    String output_table_name = convertTagToString(tag);

    if (!isValidTableName(output_table_name)) {
      auto message = String::Format("The font %s could not be accessed.",
                                    postscriptName.Latin1().c_str());
      ScriptState::Scope scope(resolver->GetScriptState());
      resolver->Reject(V8ThrowException::CreateTypeError(
          resolver->GetScriptState()->GetIsolate(), message));
      return;
    };

    if (!table_size) {
      // If table is not found or has size zero, skip it.
      continue;
    }

    // TODO(https://crbug.com/1069900): getTableData makes a copy of the bytes.
    // Stream the font bytes instead.
    Vector<char> bytes(table_size);
    size_t returned_size =
        typeface->getTableData(tag, 0ULL, table_size, bytes.data());
    DCHECK_EQ(table_size, returned_size);

    scoped_refptr<RawData> raw_data = RawData::Create();
    bytes.swap(*raw_data->MutableData());
    auto blob_data = std::make_unique<BlobData>();
    blob_data->AppendData(std::move(raw_data));

    auto* blob = MakeGarbageCollected<Blob>(
        BlobDataHandle::Create(std::move(blob_data), table_size));
    output->Set(output_table_name, blob);
  }

  auto* map = MakeGarbageCollected<FontTableMap>(*output);
  resolver->Resolve(map);
}

void FontMetadata::Trace(blink::Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
