// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/net/cookies/cookie_store_ios_persistent.h"

#import <Foundation/Foundation.h>

#include <memory>

#include "base/bind.h"
#include "base/threading/thread_checker.h"
#import "ios/net/cookies/ns_http_system_cookie_store.h"
#import "ios/net/cookies/system_cookie_util.h"
#include "net/cookies/cookie_monster.h"
#include "net/log/net_log.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace net {

namespace {
// Add metrics reporting to GetCookieListWithOptionsAsync cookie monster
// callback.
void CookieListCallbackWithMetricsLogging(
    CookieMonster::GetCookieListCallback callback,
    const CookieList& cookies,
    const CookieStatusList& excluded_cookies) {
  net::ReportGetCookiesForURLResult(SystemCookieStoreType::kCookieMonster,
                                    !cookies.empty());
  if (!callback.is_null()) {
    std::move(callback).Run(cookies, excluded_cookies);
  }
}
}  // namespace

#pragma mark -
#pragma mark CookieStoreIOSPersistent

CookieStoreIOSPersistent::CookieStoreIOSPersistent(
    net::CookieMonster::PersistentCookieStore* persistent_store,
    NetLog* net_log)
    : CookieStoreIOS(persistent_store,
                     std::make_unique<net::NSHTTPSystemCookieStore>(),
                     net_log) {}

CookieStoreIOSPersistent::CookieStoreIOSPersistent(
    net::CookieMonster::PersistentCookieStore* persistent_store,
    std::unique_ptr<SystemCookieStore> system_store,
    NetLog* net_log)
    : CookieStoreIOS(persistent_store, std::move(system_store), net_log) {}

CookieStoreIOSPersistent::~CookieStoreIOSPersistent() {}

#pragma mark -
#pragma mark CookieStoreIOSPersistent methods

void CookieStoreIOSPersistent::SetCanonicalCookieAsync(
    std::unique_ptr<CanonicalCookie> cookie,
    std::string source_scheme,
    const net::CookieOptions& options,
    SetCookiesCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  cookie_monster()->SetCanonicalCookieAsync(
      std::move(cookie), std::move(source_scheme), options,
      WrapSetCallback(std::move(callback)));
}

void CookieStoreIOSPersistent::GetCookieListWithOptionsAsync(
    const GURL& url,
    const net::CookieOptions& options,
    GetCookieListCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ReportGetCookiesForURLCall(SystemCookieStoreType::kCookieMonster);
  cookie_monster()->GetCookieListWithOptionsAsync(
      url, options,
      base::BindOnce(&CookieListCallbackWithMetricsLogging,
                     base::Passed(&callback)));
}

void CookieStoreIOSPersistent::GetAllCookiesAsync(
    GetCookieListCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  cookie_monster()->GetAllCookiesAsync(std::move(callback));
}

void CookieStoreIOSPersistent::DeleteCanonicalCookieAsync(
    const CanonicalCookie& cookie,
    DeleteCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  cookie_monster()->DeleteCanonicalCookieAsync(
      cookie, WrapDeleteCallback(std::move(callback)));
}

void CookieStoreIOSPersistent::DeleteAllCreatedInTimeRangeAsync(
    const net::CookieDeletionInfo::TimeRange& creation_range,
    DeleteCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (metrics_enabled())
    ResetCookieCountMetrics();

  cookie_monster()->DeleteAllCreatedInTimeRangeAsync(
      creation_range, WrapDeleteCallback(std::move(callback)));
}

void CookieStoreIOSPersistent::DeleteAllMatchingInfoAsync(
    CookieDeletionInfo delete_info,
    DeleteCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (metrics_enabled())
    ResetCookieCountMetrics();

  cookie_monster()->DeleteAllMatchingInfoAsync(
      std::move(delete_info), WrapDeleteCallback(std::move(callback)));
}

void CookieStoreIOSPersistent::DeleteSessionCookiesAsync(
    DeleteCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (metrics_enabled())
    ResetCookieCountMetrics();

  cookie_monster()->DeleteSessionCookiesAsync(
      WrapDeleteCallback(std::move(callback)));
}

#pragma mark -
#pragma mark Private methods

void CookieStoreIOSPersistent::WriteToCookieMonster(NSArray* system_cookies) {}

void CookieStoreIOSPersistent::OnSystemCookiesChanged() {}

}  // namespace net
