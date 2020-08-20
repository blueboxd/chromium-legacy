// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <CoreLocation/CoreLocation.h>
#import <memory>

#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/geolocation/geolocation_system_permission_mac.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"

class SystemGeolocationPermissionsManagerImpl;

@interface SystemGeolocationPermissionsDelegate
    : NSObject <CLLocationManagerDelegate> {
  bool _permissionReceived;
  bool _hasPermission;
  base::WeakPtr<SystemGeolocationPermissionsManagerImpl> _manager;
}

- (id)initWithManager:
    (base::WeakPtr<SystemGeolocationPermissionsManagerImpl>)manager;

// CLLocationManagerDelegate
- (void)locationManager:(CLLocationManager*)manager
    didChangeAuthorizationStatus:(CLAuthorizationStatus)status;
- (bool)hasPermission;
- (bool)permissionReceived;
@end

class SystemGeolocationPermissionsManagerImpl
    : public GeolocationSystemPermissionManager {
 public:
  SystemGeolocationPermissionsManagerImpl() {
    location_manager_.reset([[CLLocationManager alloc] init]);
    delegate_.reset([[SystemGeolocationPermissionsDelegate alloc]
        initWithManager:weak_ptr_factory_.GetWeakPtr()]);
    location_manager_.get().delegate = delegate_;
  }

  ~SystemGeolocationPermissionsManagerImpl() override = default;

  void PermissionUpdated() {
    for (Browser* browser : *BrowserList::GetInstance()) {
      LocationBar* location_bar = browser->window()->GetLocationBar();
      if (location_bar)
        location_bar->UpdateContentSettingsIcons();
    }
  }

  SystemPermissionStatus GetSystemPermission() override {
    if (![delegate_ permissionReceived])
      return SystemPermissionStatus::kNotDetermined;

    if ([delegate_ hasPermission])
      return SystemPermissionStatus::kAllowed;

    return SystemPermissionStatus::kDenied;
  }

 private:
  base::scoped_nsobject<SystemGeolocationPermissionsDelegate> delegate_;
  base::scoped_nsobject<CLLocationManager> location_manager_;
  base::WeakPtrFactory<SystemGeolocationPermissionsManagerImpl>
      weak_ptr_factory_{this};
};

// static
std::unique_ptr<GeolocationSystemPermissionManager>
GeolocationSystemPermissionManager::Create() {
  return std::make_unique<SystemGeolocationPermissionsManagerImpl>();
}

GeolocationSystemPermissionManager::~GeolocationSystemPermissionManager() =
    default;

@implementation SystemGeolocationPermissionsDelegate

- (id)initWithManager:
    (base::WeakPtr<SystemGeolocationPermissionsManagerImpl>)Manager {
  if (self = [super init]) {
    _permissionReceived = false;
    _hasPermission = false;
    _manager = Manager;
  }
  return self;
}

- (void)locationManager:(CLLocationManager*)manager
    didChangeAuthorizationStatus:(CLAuthorizationStatus)status {
  _permissionReceived = true;
  if (@available(macOS 10.12.0, *)) {
    if (status == kCLAuthorizationStatusAuthorizedAlways)
      _hasPermission = true;
    else
      _hasPermission = false;
  } else {
    if (status == kCLAuthorizationStatusAuthorized)
      _hasPermission = true;
    else
      _hasPermission = false;
  }
  _manager->PermissionUpdated();
}

- (bool)hasPermission {
  return _hasPermission;
}

- (bool)permissionReceived {
  return _permissionReceived;
}

@end