// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_GCPW_VERSION_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_GCPW_VERSION_H_

#include <array>
#include <string>

namespace credential_provider {

// A structure to hold the version of GCPW.
class GcpwVersion {
 public:
  // Create a default with the current GCPW version.
  GcpwVersion();

  // Construct using the given version string specified in
  // "major.minor.build.patch" format.
  GcpwVersion(const std::string& version_str);

  // Copy constructor.
  GcpwVersion(const GcpwVersion& other);

  // Returns a formatted string.
  std::string ToString() const;

  // Returns major component of version.
  unsigned major() const;

  // Returns minor component of version.
  unsigned minor() const;

  // Returns build component of version.
  unsigned build() const;

  // Returns patch component of version.
  unsigned patch() const;

  bool operator==(const GcpwVersion& other) const;

  // Assignment operator.
  GcpwVersion& operator=(const GcpwVersion& other);

  // Returns true when this version is less than |other| version.
  bool operator<(const GcpwVersion& other) const;

 private:
  std::array<unsigned, 4> version_;
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_GCPW_VERSION_H_
