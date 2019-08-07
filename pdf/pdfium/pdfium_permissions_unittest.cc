// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_permissions.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_pdf {

namespace {

constexpr auto kPermCopy = PDFEngine::PERMISSION_COPY;
constexpr auto kPermCopya11y = PDFEngine::PERMISSION_COPY_ACCESSIBLE;
constexpr auto kPermPrintHigh = PDFEngine::PERMISSION_PRINT_HIGH_QUALITY;
constexpr auto kPermPrintLow = PDFEngine::PERMISSION_PRINT_LOW_QUALITY;

constexpr uint32_t GeneratePermissions2(uint32_t permissions) {
  constexpr uint32_t kBasePermissions = 0xffffffc0;
  return kBasePermissions | permissions;
}

constexpr uint32_t GeneratePermissions3(uint32_t permissions) {
  constexpr uint32_t kBasePermissions = 0xfffff0c0;
  return kBasePermissions | permissions;
}

// Sanity check the permission constants are correct.
static_assert(kPDFPermissionCopyAccessibleMask == 0x200, "Wrong permission");
static_assert(kPDFPermissionCopyMask == 0x10, "Wrong permission");
static_assert(kPDFPermissionPrintHighQualityMask == 0x800, "Wrong permission");
static_assert(kPDFPermissionPrintLowQualityMask == 0x4, "Wrong permission");

// Sanity check the permission generation functions above do the right thing.
static_assert(GeneratePermissions2(0) == 0xffffffc0, "Wrong permission");
static_assert(GeneratePermissions2(kPDFPermissionCopyMask |
                                   kPDFPermissionPrintLowQualityMask) ==
                  0xffffffd4,
              "Wrong permission");
static_assert(GeneratePermissions3(0) == 0xfffff0c0, "Wrong permission");
static_assert(GeneratePermissions3(kPDFPermissionCopyAccessibleMask |
                                   kPDFPermissionCopyMask |
                                   kPDFPermissionPrintHighQualityMask |
                                   kPDFPermissionPrintLowQualityMask) ==
                  0xfffffad4,
              "Wrong permission");

TEST(PDFiumPermissionTest, InvalidSecurityHandler) {
  constexpr int kPDFiumUnknownRevision = -1;
  constexpr uint32_t kNoPermissions = 0;
  auto unknown_perms = PDFiumPermissions::CreateForTesting(
      kPDFiumUnknownRevision, kNoPermissions);
  EXPECT_TRUE(unknown_perms.HasPermission(kPermCopy));
  EXPECT_TRUE(unknown_perms.HasPermission(kPermCopya11y));
  EXPECT_TRUE(unknown_perms.HasPermission(kPermPrintLow));
  EXPECT_TRUE(unknown_perms.HasPermission(kPermPrintHigh));

  constexpr int kInvalidRevision = 1;
  auto obsolete_perms =
      PDFiumPermissions::CreateForTesting(kInvalidRevision, kNoPermissions);
  EXPECT_TRUE(obsolete_perms.HasPermission(kPermCopy));
  EXPECT_TRUE(obsolete_perms.HasPermission(kPermCopya11y));
  EXPECT_TRUE(obsolete_perms.HasPermission(kPermPrintLow));
  EXPECT_TRUE(obsolete_perms.HasPermission(kPermPrintHigh));
}

TEST(PDFiumPermissionTest, Revision2SecurityHandler) {
  uint32_t permissions = GeneratePermissions2(0);
  auto no_perms = PDFiumPermissions::CreateForTesting(2, permissions);
  EXPECT_FALSE(no_perms.HasPermission(kPermCopy));
  // TODO(crbug.com/989408) Should be the same as |PERMISSION_COPY|.
  EXPECT_TRUE(no_perms.HasPermission(kPermCopya11y));
  EXPECT_FALSE(no_perms.HasPermission(kPermPrintLow));
  EXPECT_FALSE(no_perms.HasPermission(kPermPrintHigh));

  permissions = GeneratePermissions2(kPDFPermissionCopyMask |
                                     kPDFPermissionPrintLowQualityMask);
  auto all_known_perms = PDFiumPermissions::CreateForTesting(2, permissions);
  EXPECT_TRUE(all_known_perms.HasPermission(kPermCopy));
  EXPECT_TRUE(all_known_perms.HasPermission(kPermCopya11y));
  EXPECT_TRUE(all_known_perms.HasPermission(kPermPrintLow));
  EXPECT_TRUE(all_known_perms.HasPermission(kPermPrintHigh));

  permissions = GeneratePermissions2(kPDFPermissionCopyMask);
  auto no_print_perms = PDFiumPermissions::CreateForTesting(2, permissions);
  EXPECT_TRUE(no_print_perms.HasPermission(kPermCopy));
  EXPECT_TRUE(no_print_perms.HasPermission(kPermCopya11y));
  EXPECT_FALSE(no_print_perms.HasPermission(kPermPrintLow));
  EXPECT_FALSE(no_print_perms.HasPermission(kPermPrintHigh));

  permissions = GeneratePermissions2(kPDFPermissionPrintLowQualityMask);
  auto no_copy_perms = PDFiumPermissions::CreateForTesting(2, permissions);
  EXPECT_FALSE(no_copy_perms.HasPermission(kPermCopy));
  // TODO(crbug.com/989408) Should be the same as |PERMISSION_COPY|.
  EXPECT_TRUE(no_copy_perms.HasPermission(kPermCopya11y));
  EXPECT_TRUE(no_copy_perms.HasPermission(kPermPrintLow));
  EXPECT_TRUE(no_copy_perms.HasPermission(kPermPrintHigh));
}

TEST(PDFiumPermissionTest, Revision3SecurityHandler) {
  uint32_t permissions = GeneratePermissions3(0);
  auto no_perms = PDFiumPermissions::CreateForTesting(3, permissions);
  EXPECT_FALSE(no_perms.HasPermission(kPermCopy));
  EXPECT_FALSE(no_perms.HasPermission(kPermCopya11y));
  EXPECT_FALSE(no_perms.HasPermission(kPermPrintLow));
  EXPECT_FALSE(no_perms.HasPermission(kPermPrintHigh));

  permissions = GeneratePermissions3(
      kPDFPermissionCopyAccessibleMask | kPDFPermissionCopyMask |
      kPDFPermissionPrintHighQualityMask | kPDFPermissionPrintLowQualityMask);
  auto all_known_perms = PDFiumPermissions::CreateForTesting(3, permissions);
  EXPECT_TRUE(all_known_perms.HasPermission(kPermCopy));
  EXPECT_TRUE(all_known_perms.HasPermission(kPermCopya11y));
  EXPECT_TRUE(all_known_perms.HasPermission(kPermPrintLow));
  EXPECT_TRUE(all_known_perms.HasPermission(kPermPrintHigh));

  permissions = GeneratePermissions3(kPDFPermissionCopyAccessibleMask |
                                     kPDFPermissionCopyMask);
  auto copy_no_print_perms =
      PDFiumPermissions::CreateForTesting(3, permissions);
  EXPECT_TRUE(copy_no_print_perms.HasPermission(kPermCopy));
  EXPECT_TRUE(copy_no_print_perms.HasPermission(kPermCopya11y));
  EXPECT_FALSE(copy_no_print_perms.HasPermission(kPermPrintLow));
  EXPECT_FALSE(copy_no_print_perms.HasPermission(kPermPrintHigh));

  permissions = GeneratePermissions3(kPDFPermissionCopyAccessibleMask |
                                     kPDFPermissionCopyMask |
                                     kPDFPermissionPrintLowQualityMask);
  auto copy_low_print_perms =
      PDFiumPermissions::CreateForTesting(3, permissions);
  EXPECT_TRUE(copy_low_print_perms.HasPermission(kPermCopy));
  EXPECT_TRUE(copy_low_print_perms.HasPermission(kPermCopya11y));
  EXPECT_TRUE(copy_low_print_perms.HasPermission(kPermPrintLow));
  EXPECT_FALSE(copy_low_print_perms.HasPermission(kPermPrintHigh));

  permissions = GeneratePermissions3(kPDFPermissionPrintHighQualityMask |
                                     kPDFPermissionPrintLowQualityMask);
  auto print_no_copy_perms =
      PDFiumPermissions::CreateForTesting(3, permissions);
  EXPECT_FALSE(print_no_copy_perms.HasPermission(kPermCopy));
  EXPECT_FALSE(print_no_copy_perms.HasPermission(kPermCopya11y));
  EXPECT_TRUE(print_no_copy_perms.HasPermission(kPermPrintLow));
  EXPECT_TRUE(print_no_copy_perms.HasPermission(kPermPrintHigh));

  permissions = GeneratePermissions3(kPDFPermissionCopyAccessibleMask |
                                     kPDFPermissionPrintHighQualityMask |
                                     kPDFPermissionPrintLowQualityMask);
  auto print_a11y_copy_perms =
      PDFiumPermissions::CreateForTesting(3, permissions);
  EXPECT_FALSE(print_a11y_copy_perms.HasPermission(kPermCopy));
  EXPECT_TRUE(print_a11y_copy_perms.HasPermission(kPermCopya11y));
  EXPECT_TRUE(print_a11y_copy_perms.HasPermission(kPermPrintLow));
  EXPECT_TRUE(print_a11y_copy_perms.HasPermission(kPermPrintHigh));
}

}  // namespace

}  // namespace chrome_pdf
