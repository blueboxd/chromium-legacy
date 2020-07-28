// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/elf_reader.h"

#include <dlfcn.h>

#include <cstdint>

#include "base/debug/test_elf_image_builder.h"
#include "base/files/memory_mapped_file.h"
#include "base/native_library.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

extern char __executable_start;

namespace base {
namespace debug {

namespace {
constexpr uint8_t kBuildIdBytes[] = {0xab, 0xcd, 0x12, 0x34};
constexpr const char kBuildIdHexString[] = "ABCD1234";
constexpr const char kBuildIdHexStringLower[] = "ABCD1234";
}  // namespace

TEST(ElfReaderTest, ReadElfBuildIdUppercase) {
  TestElfImage image =
      TestElfImageBuilder()
          .AddLoadSegment(PF_R | PF_X, /* size = */ 2000)
          .AddNoteSegment(NT_GNU_BUILD_ID, "GNU", kBuildIdBytes)
          .Build();

  ElfBuildIdBuffer build_id;
  size_t build_id_size = ReadElfBuildId(image.elf_start(), true, build_id);
  EXPECT_EQ(8u, build_id_size);
  EXPECT_EQ(kBuildIdHexString, StringPiece(&build_id[0], build_id_size));
}

TEST(ElfReaderTest, ReadElfBuildIdLowercase) {
  TestElfImage image =
      TestElfImageBuilder()
          .AddLoadSegment(PF_R | PF_X, /* size = */ 2000)
          .AddNoteSegment(NT_GNU_BUILD_ID, "GNU", kBuildIdBytes)
          .Build();

  ElfBuildIdBuffer build_id;
  size_t build_id_size = ReadElfBuildId(image.elf_start(), false, build_id);
  EXPECT_EQ(8u, build_id_size);
  EXPECT_EQ(ToLowerASCII(kBuildIdHexStringLower),
            StringPiece(&build_id[0], build_id_size));
}

TEST(ElfReaderTest, ReadElfBuildIdMultipleNotes) {
  constexpr uint8_t kOtherNoteBytes[] = {0xef, 0x56};

  TestElfImage image =
      TestElfImageBuilder()
          .AddLoadSegment(PF_R | PF_X, /* size = */ 2000)
          .AddNoteSegment(NT_GNU_BUILD_ID + 1, "ABC", kOtherNoteBytes)
          .AddNoteSegment(NT_GNU_BUILD_ID, "GNU", kBuildIdBytes)
          .Build();

  ElfBuildIdBuffer build_id;
  size_t build_id_size = ReadElfBuildId(image.elf_start(), true, build_id);
  EXPECT_EQ(8u, build_id_size);
  EXPECT_EQ(kBuildIdHexString, StringPiece(&build_id[0], build_id_size));
}

TEST(ElfReaderTest, ReadElfBuildIdWrongName) {
  TestElfImage image =
      TestElfImageBuilder()
          .AddLoadSegment(PF_R | PF_X, /* size = */ 2000)
          .AddNoteSegment(NT_GNU_BUILD_ID, "ABC", kBuildIdBytes)
          .Build();

  ElfBuildIdBuffer build_id;
  size_t build_id_size = ReadElfBuildId(image.elf_start(), true, build_id);
  EXPECT_EQ(0u, build_id_size);
}

TEST(ElfReaderTest, ReadElfBuildIdWrongType) {
  TestElfImage image =
      TestElfImageBuilder()
          .AddLoadSegment(PF_R | PF_X, /* size = */ 2000)
          .AddNoteSegment(NT_GNU_BUILD_ID + 1, "GNU", kBuildIdBytes)
          .Build();

  ElfBuildIdBuffer build_id;
  size_t build_id_size = ReadElfBuildId(image.elf_start(), true, build_id);
  EXPECT_EQ(0u, build_id_size);
}

TEST(ElfReaderTest, ReadElfBuildIdNoBuildId) {
  TestElfImage image = TestElfImageBuilder()
                           .AddLoadSegment(PF_R | PF_X, /* size = */ 2000)
                           .Build();

  ElfBuildIdBuffer build_id;
  size_t build_id_size = ReadElfBuildId(image.elf_start(), true, build_id);
  EXPECT_EQ(0u, build_id_size);
}

TEST(ElfReaderTest, ReadElfBuildIdForCurrentElfImage) {
  ElfBuildIdBuffer build_id;
  size_t build_id_size = ReadElfBuildId(&__executable_start, true, build_id);
  ASSERT_NE(build_id_size, 0u);

#if defined(OFFICIAL_BUILD)
  constexpr size_t kExpectedBuildIdStringLength = 40;  // SHA1 hash in hex.
#else
  constexpr size_t kExpectedBuildIdStringLength = 16;  // 64-bit int in hex.
#endif

  EXPECT_EQ(kExpectedBuildIdStringLength, build_id_size);
  for (size_t i = 0; i < build_id_size; ++i) {
    char c = build_id[i];
    EXPECT_TRUE(IsHexDigit(c));
    EXPECT_FALSE(IsAsciiLower(c));
  }
}

TEST(ElfReaderTest, ReadElfLibraryName) {
  TestElfImage image = TestElfImageBuilder()
                           .AddLoadSegment(PF_R | PF_X, /* size = */ 2000)
                           .AddSoName("mysoname")
                           .Build();

  Optional<StringPiece> library_name = ReadElfLibraryName(image.elf_start());
  ASSERT_NE(nullopt, library_name);
  EXPECT_EQ("mysoname", *library_name);
}

TEST(ElfReaderTest, ReadElfLibraryNameNoSoName) {
  TestElfImage image = TestElfImageBuilder()
                           .AddLoadSegment(PF_R | PF_X, /* size = */ 2000)
                           .Build();

  Optional<StringPiece> library_name = ReadElfLibraryName(image.elf_start());
  EXPECT_EQ(nullopt, library_name);
}

TEST(ElfReaderTest, ReadElfLibraryNameForCurrentElfImage) {
#if defined(OS_ANDROID)
  // On Android the library loader memory maps the full so file.
  const char kLibraryName[] = "libbase_unittests__library";
  const void* addr = &__executable_start;
#else
  const char kLibraryName[] = MALLOC_WRAPPER_LIB;
  // On Linux the executable does not contain soname and is not mapped till
  // dynamic segment. So, use malloc wrapper so file on which the test already
  // depends on.
  // Find any symbol in the loaded file.
  //
  NativeLibraryLoadError error;
  NativeLibrary library =
      LoadNativeLibrary(base::FilePath(kLibraryName), &error);
  void* init_addr =
      GetFunctionPointerFromNativeLibrary(library, "MallocWrapper");

  // Use this symbol to get full path to the loaded library.
  Dl_info info;
  int res = dladdr(init_addr, &info);
  ASSERT_NE(0, res);
  const void* addr = info.dli_fbase;
#endif

  auto name = ReadElfLibraryName(addr);
  ASSERT_TRUE(name);
  EXPECT_NE(std::string::npos, name->find(kLibraryName))
      << "Library name " << *name << " doesn't contain expected "
      << kLibraryName;

#if !defined(OS_ANDROID)
  UnloadNativeLibrary(library);
#endif
}

}  // namespace debug
}  // namespace base
