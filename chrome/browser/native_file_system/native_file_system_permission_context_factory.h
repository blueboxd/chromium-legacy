// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_PERMISSION_CONTEXT_FACTORY_H_
#define CHROME_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_PERMISSION_CONTEXT_FACTORY_H_

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/content/refcounted_browser_context_keyed_service_factory.h"

class ChromeNativeFileSystemPermissionContext;

// Factory to get or create an instance of
// ChromeNativeFileSystemPermissionContext from a Profile.
class NativeFileSystemPermissionContextFactory
    : public RefcountedBrowserContextKeyedServiceFactory {
 public:
  static scoped_refptr<ChromeNativeFileSystemPermissionContext> GetForProfile(
      content::BrowserContext* profile);
  static scoped_refptr<ChromeNativeFileSystemPermissionContext>
  GetForProfileIfExists(content::BrowserContext* profile);
  static NativeFileSystemPermissionContextFactory* GetInstance();

 private:
  friend class base::NoDestructor<NativeFileSystemPermissionContextFactory>;

  NativeFileSystemPermissionContextFactory();
  ~NativeFileSystemPermissionContextFactory() override;

  // RefcountedBrowserContextKeyedServiceFactory:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;

  DISALLOW_COPY_AND_ASSIGN(NativeFileSystemPermissionContextFactory);
};

#endif  // CHROME_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_PERMISSION_CONTEXT_FACTORY_H_
