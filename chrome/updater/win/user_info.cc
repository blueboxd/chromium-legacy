// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/user_info.h"

#include "base/check.h"
#include "base/strings/string16.h"
#include "chrome/updater/win/util.h"

namespace updater {

namespace {

// Sid of NT AUTHORITY\SYSTEM user.
constexpr base::char16 kSystemPrincipalSid[] = L"S-1-5-18";

}  // namespace

HRESULT GetProcessUser(base::string16* name,
                       base::string16* domain,
                       base::string16* sid) {
  CSid current_sid;

  HRESULT hr = GetProcessUserSid(&current_sid);
  if (FAILED(hr))
    return hr;

  if (sid)
    *sid = current_sid.Sid();
  if (name)
    *name = current_sid.AccountName();
  if (domain)
    *domain = current_sid.Domain();

  return S_OK;
}

HRESULT GetProcessUserSid(CSid* sid) {
  DCHECK(sid);

  CAccessToken token;
  if (!token.GetProcessToken(TOKEN_QUERY) || !token.GetUser(sid)) {
    HRESULT hr = HRESULTFromLastError();
    base::string16 thread_sid;
    DCHECK(FAILED(GetThreadUserSid(&thread_sid)));
    return hr;
  }

  return S_OK;
}

bool IsLocalSystemUser() {
  base::string16 user_sid;
  HRESULT hr = GetProcessUser(nullptr, nullptr, &user_sid);
  return SUCCEEDED(hr) && user_sid.compare(kSystemPrincipalSid) == 0;
}

HRESULT GetThreadUserSid(base::string16* sid) {
  DCHECK(sid);
  CAccessToken access_token;
  CSid user_sid;
  if (access_token.GetThreadToken(TOKEN_READ) &&
      access_token.GetUser(&user_sid)) {
    *sid = user_sid.Sid();
    return S_OK;
  }

  return HRESULTFromLastError();
}

}  // namespace updater
