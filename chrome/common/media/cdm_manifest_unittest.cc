// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/media/cdm_manifest.h"

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "content/public/common/cdm_info.h"
#include "media/cdm/api/content_decryption_module.h"
#include "media/cdm/supported_cdm_versions.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::CdmCapability;

namespace {

// These names must match what is used in cdm_manifest.cc.
const char kCdmModuleVersionsName[] = "x-cdm-module-versions";
const char kCdmInterfaceVersionsName[] = "x-cdm-interface-versions";
const char kCdmHostVersionsName[] = "x-cdm-host-versions";
const char kCdmCodecsListName[] = "x-cdm-codecs";
const char kCdmPersistentLicenseSupportName[] =
    "x-cdm-persistent-license-support";
const char kCdmSupportedEncryptionSchemesName[] =
    "x-cdm-supported-encryption-schemes";
const char kCdmSupportedCdmProxyProtocolsName[] =
    "x-cdm-supported-cdm-proxy-protocols";

// Version checking does change over time. Deriving these values from constants
// in the code to ensure they change when the CDM interface changes.
// |kSupportedCdmInterfaceVersion| and |kSupportedCdmHostVersion| are the
// minimum versions supported. There may be versions after them that are also
// supported.
constexpr int kSupportedCdmModuleVersion = CDM_MODULE_VERSION;
constexpr int kSupportedCdmInterfaceVersion =
    media::kSupportedCdmInterfaceVersions[0].version;
static_assert(media::kSupportedCdmInterfaceVersions[0].enabled,
              "kSupportedCdmInterfaceVersion is not enabled by default.");
constexpr int kSupportedCdmHostVersion = media::kMinSupportedCdmHostVersion;

// Make a string of the values from 0 up to and including |item|.
std::string MakeStringList(int item) {
  DCHECK_GT(item, 0);
  std::vector<std::string> parts;
  for (int i = 0; i <= item; ++i) {
    parts.push_back(base::NumberToString(i));
  }
  return base::JoinString(parts, ",");
}

base::Value MakeListValue(const std::string& item) {
  base::Value list(base::Value::Type::LIST);
  list.GetList().push_back(base::Value(item));
  return list;
}

base::Value MakeListValue(const std::string& item1, const std::string& item2) {
  base::Value list(base::Value::Type::LIST);
  list.GetList().push_back(base::Value(item1));
  list.GetList().push_back(base::Value(item2));
  return list;
}

// Create a default manifest with valid values for all entries.
base::Value DefaultManifest() {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey(kCdmCodecsListName, "vp8,vp9.0,avc1");
  dict.SetBoolKey(kCdmPersistentLicenseSupportName, true);
  dict.SetKey(kCdmSupportedEncryptionSchemesName,
              MakeListValue("cenc", "cbcs"));
  dict.SetKey(kCdmSupportedCdmProxyProtocolsName, MakeListValue("intel"));

  // The following are dependent on what the current code supports.
  EXPECT_TRUE(media::IsSupportedCdmModuleVersion(kSupportedCdmModuleVersion));
  EXPECT_TRUE(media::IsSupportedAndEnabledCdmInterfaceVersion(
      kSupportedCdmInterfaceVersion));
  EXPECT_TRUE(media::IsSupportedCdmHostVersion(kSupportedCdmHostVersion));
  dict.SetStringKey(kCdmModuleVersionsName,
                    base::NumberToString(kSupportedCdmModuleVersion));
  dict.SetStringKey(kCdmInterfaceVersionsName,
                    base::NumberToString(kSupportedCdmInterfaceVersion));
  dict.SetStringKey(kCdmHostVersionsName,
                    base::NumberToString(kSupportedCdmHostVersion));
  return dict;
}

void CheckCodecs(const std::vector<media::VideoCodec>& actual,
                 const std::vector<media::VideoCodec>& expected) {
  EXPECT_EQ(expected.size(), actual.size());
  for (const auto& codec : expected) {
    EXPECT_TRUE(base::Contains(actual, codec));
  }
}

}  // namespace

TEST(CdmManifestTest, IsCompatibleWithChrome) {
  base::Value manifest(DefaultManifest());
  EXPECT_TRUE(IsCdmManifestCompatibleWithChrome(manifest));
}

TEST(CdmManifestTest, InCompatibleModuleVersion) {
  const int kUnsupportedModuleVersion = 0;
  EXPECT_FALSE(media::IsSupportedCdmModuleVersion(kUnsupportedModuleVersion));

  auto manifest = DefaultManifest();
  manifest.SetStringKey(kCdmModuleVersionsName,
                        base::NumberToString(kUnsupportedModuleVersion));
  EXPECT_FALSE(IsCdmManifestCompatibleWithChrome(std::move(manifest)));
}

TEST(CdmManifestTest, InCompatibleInterfaceVersion) {
  const int kUnsupportedInterfaceVersion = kSupportedCdmInterfaceVersion - 1;
  EXPECT_FALSE(media::IsSupportedAndEnabledCdmInterfaceVersion(
      kUnsupportedInterfaceVersion));

  auto manifest = DefaultManifest();
  manifest.SetStringKey(kCdmInterfaceVersionsName,
                        base::NumberToString(kUnsupportedInterfaceVersion));
  EXPECT_FALSE(IsCdmManifestCompatibleWithChrome(std::move(manifest)));
}

TEST(CdmManifestTest, InCompatibleHostVersion) {
  const int kUnsupportedHostVersion = kSupportedCdmHostVersion - 1;
  EXPECT_FALSE(media::IsSupportedCdmHostVersion(kUnsupportedHostVersion));

  auto manifest = DefaultManifest();
  manifest.SetStringKey(kCdmHostVersionsName,
                        base::NumberToString(kUnsupportedHostVersion));
  EXPECT_FALSE(IsCdmManifestCompatibleWithChrome(std::move(manifest)));
}

TEST(CdmManifestTest, IsCompatibleWithMultipleValues) {
  auto manifest = DefaultManifest();
  manifest.SetStringKey(kCdmModuleVersionsName,
                        MakeStringList(kSupportedCdmModuleVersion));
  manifest.SetStringKey(kCdmInterfaceVersionsName,
                        MakeStringList(kSupportedCdmInterfaceVersion));
  manifest.SetStringKey(kCdmHostVersionsName,
                        MakeStringList(kSupportedCdmHostVersion));
  EXPECT_TRUE(IsCdmManifestCompatibleWithChrome(std::move(manifest)));
}

TEST(CdmManifestTest, ValidManifest) {
  auto manifest = DefaultManifest();
  CdmCapability capability;
  EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
}

TEST(CdmManifestTest, ManifestCodecs) {
  auto manifest = DefaultManifest();

  // Try each valid value individually.
  {
    CdmCapability capability;
    manifest.SetStringKey(kCdmCodecsListName, "vp8");
    EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
    CheckCodecs(capability.video_codecs, {media::VideoCodec::kCodecVP8});
  }
  {
    CdmCapability capability;
    manifest.SetStringKey(kCdmCodecsListName, "vp9.0");
    EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
    CheckCodecs(capability.video_codecs, {media::VideoCodec::kCodecVP9});
    EXPECT_FALSE(capability.supports_vp9_profile2);
  }
  {
    CdmCapability capability;
    manifest.SetStringKey(kCdmCodecsListName, "vp09");
    EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
    CheckCodecs(capability.video_codecs, {media::VideoCodec::kCodecVP9});
    EXPECT_TRUE(capability.supports_vp9_profile2);
  }
  {
    CdmCapability capability;
    manifest.SetStringKey(kCdmCodecsListName, "av01");
    EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
    CheckCodecs(capability.video_codecs, {media::VideoCodec::kCodecAV1});
  }
  {
    CdmCapability capability;
    manifest.SetStringKey(kCdmCodecsListName, "avc1");
    EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
    CheckCodecs(capability.video_codecs, {media::VideoCodec::kCodecH264});
  }
  {
    // Try list of everything.
    CdmCapability capability;
    manifest.SetStringKey(kCdmCodecsListName, "vp8,vp9.0,vp09,av01,avc1");
    EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
    // Note that kCodecVP9 is returned twice in the list.
    CheckCodecs(capability.video_codecs,
                {media::VideoCodec::kCodecVP8, media::VideoCodec::kCodecVP9,
                 media::VideoCodec::kCodecVP9, media::VideoCodec::kCodecAV1,
                 media::VideoCodec::kCodecH264});
    EXPECT_TRUE(capability.supports_vp9_profile2);
  }
  {
    // Note that invalid codec values are simply skipped.
    CdmCapability capability;
    manifest.SetStringKey(kCdmCodecsListName, "invalid,avc1");
    EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
    CheckCodecs(capability.video_codecs, {media::VideoCodec::kCodecH264});
  }
  {
    // Wrong types are an error.
    CdmCapability capability;
    manifest.SetBoolKey(kCdmCodecsListName, true);
    EXPECT_FALSE(ParseCdmManifest(manifest, &capability));
  }
  {
    // Missing entry is OK, but list is empty.
    CdmCapability capability;
    EXPECT_TRUE(manifest.RemoveKey(kCdmCodecsListName));
    EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
    CheckCodecs(capability.video_codecs, {});
  }
}

TEST(CdmManifestTest, ManifestEncryptionSchemes) {
  auto manifest = DefaultManifest();

  // Try each valid value individually.
  {
    CdmCapability capability;
    manifest.SetKey(kCdmSupportedEncryptionSchemesName, MakeListValue("cenc"));
    EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
    EXPECT_EQ(capability.encryption_schemes.size(), 1u);
    EXPECT_TRUE(base::Contains(capability.encryption_schemes,
                               media::EncryptionMode::kCenc));
    EXPECT_FALSE(base::Contains(capability.encryption_schemes,
                                media::EncryptionMode::kCbcs));
  }
  {
    CdmCapability capability;
    manifest.SetKey(kCdmSupportedEncryptionSchemesName, MakeListValue("cbcs"));
    EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
    EXPECT_EQ(capability.encryption_schemes.size(), 1u);
    EXPECT_FALSE(base::Contains(capability.encryption_schemes,
                                media::EncryptionMode::kCenc));
    EXPECT_TRUE(base::Contains(capability.encryption_schemes,
                               media::EncryptionMode::kCbcs));
  }
  {
    // Try multiple valid entries.
    CdmCapability capability;
    manifest.SetKey(kCdmSupportedEncryptionSchemesName,
                    MakeListValue("cenc", "cbcs"));
    EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
    EXPECT_EQ(capability.encryption_schemes.size(), 2u);
    EXPECT_TRUE(base::Contains(capability.encryption_schemes,
                               media::EncryptionMode::kCenc));
    EXPECT_TRUE(base::Contains(capability.encryption_schemes,
                               media::EncryptionMode::kCbcs));
  }
  {
    // Invalid encryption schemes are ignored. However, if value specified then
    // there must be at least 1 valid value.
    CdmCapability capability;
    manifest.SetKey(kCdmSupportedEncryptionSchemesName,
                    MakeListValue("invalid"));
    EXPECT_FALSE(ParseCdmManifest(manifest, &capability));
  }
  {
    CdmCapability capability;
    manifest.SetKey(kCdmSupportedEncryptionSchemesName,
                    MakeListValue("invalid", "cenc"));
    EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
    EXPECT_TRUE(base::Contains(capability.encryption_schemes,
                               media::EncryptionMode::kCenc));
    EXPECT_FALSE(base::Contains(capability.encryption_schemes,
                                media::EncryptionMode::kCbcs));
  }
  {
    // Wrong types are an error.
    CdmCapability capability;
    manifest.SetBoolKey(kCdmSupportedEncryptionSchemesName, true);
    EXPECT_FALSE(ParseCdmManifest(manifest, &capability));
  }
  {
    // Missing values default to "cenc".
    CdmCapability capability;
    EXPECT_TRUE(manifest.RemoveKey(kCdmSupportedEncryptionSchemesName));
    EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
    EXPECT_TRUE(base::Contains(capability.encryption_schemes,
                               media::EncryptionMode::kCenc));
    EXPECT_FALSE(base::Contains(capability.encryption_schemes,
                                media::EncryptionMode::kCbcs));
  }
}

TEST(CdmManifestTest, ManifestSessionTypes) {
  auto manifest = DefaultManifest();

  {
    // Try false (persistent license not supported).
    CdmCapability capability;
    manifest.SetBoolKey(kCdmPersistentLicenseSupportName, false);
    EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
    EXPECT_EQ(capability.session_types.size(), 1u);
    EXPECT_TRUE(base::Contains(capability.session_types,
                               media::CdmSessionType::kTemporary));
    EXPECT_FALSE(base::Contains(capability.session_types,
                                media::CdmSessionType::kPersistentLicense));
  }
  {
    // Try true (persistent license is supported).
    CdmCapability capability;
    manifest.SetBoolKey(kCdmPersistentLicenseSupportName, true);
    EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
    EXPECT_EQ(capability.session_types.size(), 2u);
    EXPECT_TRUE(base::Contains(capability.session_types,
                               media::CdmSessionType::kTemporary));
    EXPECT_TRUE(base::Contains(capability.session_types,
                               media::CdmSessionType::kPersistentLicense));
  }
  {
    // Wrong types are an error.
    CdmCapability capability;
    manifest.SetStringKey(kCdmPersistentLicenseSupportName, "true");
    EXPECT_FALSE(ParseCdmManifest(manifest, &capability));
  }
  {
    // Missing values default to "temporary".
    CdmCapability capability;
    EXPECT_TRUE(manifest.RemoveKey(kCdmPersistentLicenseSupportName));
    EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
    EXPECT_TRUE(base::Contains(capability.session_types,
                               media::CdmSessionType::kTemporary));
    EXPECT_FALSE(base::Contains(capability.session_types,
                                media::CdmSessionType::kPersistentLicense));
  }
}

TEST(CdmManifestTest, ManifestProxyProtocols) {
  auto manifest = DefaultManifest();

  {
    // Try only supported value.
    CdmCapability capability;
    manifest.SetKey(kCdmSupportedCdmProxyProtocolsName, MakeListValue("intel"));
    EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
    EXPECT_EQ(capability.cdm_proxy_protocols.size(), 1u);
    EXPECT_TRUE(base::Contains(capability.cdm_proxy_protocols,
                               media::CdmProxy::Protocol::kIntel));
  }
  {
    // Unrecognized values are ignored.
    CdmCapability capability;
    manifest.SetKey(kCdmSupportedCdmProxyProtocolsName,
                    MakeListValue("unknown", "intel"));
    EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
    EXPECT_EQ(capability.cdm_proxy_protocols.size(), 1u);
    EXPECT_TRUE(base::Contains(capability.cdm_proxy_protocols,
                               media::CdmProxy::Protocol::kIntel));
  }
  {
    // Wrong types are an error.
    CdmCapability capability;
    manifest.SetStringKey(kCdmSupportedCdmProxyProtocolsName, "intel");
    EXPECT_FALSE(ParseCdmManifest(manifest, &capability));
  }
  {
    // Missing values are OK.
    CdmCapability capability;
    EXPECT_TRUE(manifest.RemoveKey(kCdmSupportedCdmProxyProtocolsName));
    EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
    EXPECT_EQ(capability.cdm_proxy_protocols.size(), 0u);
  }
}
