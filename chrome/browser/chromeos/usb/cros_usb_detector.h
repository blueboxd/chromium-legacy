// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_USB_CROS_USB_DETECTOR_H_
#define CHROME_BROWSER_CHROMEOS_USB_CROS_USB_DETECTOR_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom.h"
#include "services/device/public/mojom/usb_manager.mojom.h"
#include "services/device/public/mojom/usb_manager_client.mojom.h"

namespace chromeos {

const uint8_t kInvalidUsbPortNumber = 0xff;

// Reasons the notification may be closed. These are used in histograms so do
// not remove/reorder entries. Only add at the end just before kMaxValue. Also
// remember to update the enum listing in
// tools/metrics/histograms/histograms.xml.
enum class CrosUsbNotificationClosed {
  // The notification was dismissed but not by the user (either automatically
  // or because the device was unplugged).
  kUnknown,
  // The user closed the notification via the close box.
  kByUser,
  // The user clicked on the Connect to Linux button of the notification.
  kConnectToLinux,
  // Maximum value for the enum.
  kMaxValue = kConnectToLinux
};

struct SharedUsbDeviceInfo {
  SharedUsbDeviceInfo();
  SharedUsbDeviceInfo(const SharedUsbDeviceInfo&);
  ~SharedUsbDeviceInfo();

  std::string vm_name;
  std::string guid;
  base::string16 label;
  bool shared = false;
  base::Optional<uint8_t> guest_port;
  // TODO(nverne): Add current state and errors etc.
};

class SharedUsbDeviceObserver : public base::CheckedObserver {
 public:
  // Called when the available USB devices change.
  virtual void OnSharedUsbDevicesChanged(
      std::vector<SharedUsbDeviceInfo> usb_devices) = 0;
};

// Detects USB Devices for Chrome OS and manages UI for controlling their use
// with CrOS, Web or GuestOSs.
class CrosUsbDetector : public device::mojom::UsbDeviceManagerClient {
 public:
  // Used to namespace USB notifications to avoid clashes with WebUsbDetector.
  static std::string MakeNotificationId(const std::string& guid);

  // Can return nullptr.
  static CrosUsbDetector* Get();

  CrosUsbDetector();
  ~CrosUsbDetector() override;

  void SetDeviceManagerForTesting(
      device::mojom::UsbDeviceManagerPtr device_manager);

  // Connect to the device manager to be notified of connection/removal.
  // Used during browser startup, after connection errors and to setup a fake
  // device manager during testing.
  void ConnectToDeviceManager();

  // Called by CrostiniManager when a VM starts, to attach USB devices marked as
  // shared to the VM.
  void ConnectSharedDevicesOnVmStartup();

  // device::mojom::UsbDeviceManagerClient
  void OnDeviceAdded(device::mojom::UsbDeviceInfoPtr device) override;
  void OnDeviceRemoved(device::mojom::UsbDeviceInfoPtr device) override;

  void AttachUsbDeviceToVm(
      const std::string& vm_name,
      const std::string& guid,
      crostini::CrostiniManager::CrostiniResultCallback callback);
  void DetachUsbDeviceFromVm(
      const std::string& vm_name,
      const std::string& guid,
      crostini::CrostiniManager::CrostiniResultCallback callback);

  void AddSharedUsbDeviceObserver(SharedUsbDeviceObserver* observer);
  void RemoveSharedUsbDeviceObserver(SharedUsbDeviceObserver* observer);
  void SignalSharedUsbDeviceObservers();

  std::vector<SharedUsbDeviceInfo> GetSharedUsbDevices();

 private:
  // Called after USB device access has been checked.
  void OnDeviceChecked(device::mojom::UsbDeviceInfoPtr device,
                       bool hide_notification,
                       bool allowed);

  // Allows the notification to be hidden (OnDeviceAdded without the flag calls
  // this).
  void OnDeviceAdded(device::mojom::UsbDeviceInfoPtr device,
                     bool hide_notification);
  void OnDeviceManagerConnectionError();

  // Callback listing devices attached to the machine.
  void OnListAttachedDevices(
      std::vector<device::mojom::UsbDeviceInfoPtr> devices);

  // Callback for AttachUsbDeviceToVm after opening a file handler.
  void OnAttachUsbDeviceOpened(
      const std::string& vm_name,
      device::mojom::UsbDeviceInfoPtr device,
      crostini::CrostiniManager::CrostiniResultCallback callback,
      base::File file);

  // Callbacks for when the USB device state has been updated.
  void OnUsbDeviceAttachFinished(
      const std::string& vm_name,
      const std::string& guid,
      crostini::CrostiniManager::CrostiniResultCallback callback,
      uint8_t guest_port,
      crostini::CrostiniResult result);
  void OnUsbDeviceDetachFinished(
      const std::string& vm_name,
      const std::string& guid,
      crostini::CrostiniManager::CrostiniResultCallback callback,
      uint8_t guest_port,
      crostini::CrostiniResult result);

  // Returns true when a device should show a notification when attached.
  bool ShouldShowNotification(const device::mojom::UsbDeviceInfo& device_info);
  // Returns true when a device can be shared.
  bool IsDeviceSharable(const device::mojom::UsbDeviceInfo& device_info);

  device::mojom::UsbDeviceManagerPtr device_manager_;
  mojo::AssociatedBinding<device::mojom::UsbDeviceManagerClient>
      client_binding_;

  std::vector<device::mojom::UsbDeviceFilterPtr> guest_os_classes_blocked_;
  std::vector<device::mojom::UsbDeviceFilterPtr>
      guest_os_classes_without_notif_;
  device::mojom::UsbDeviceFilterPtr adb_device_filter_;
  device::mojom::UsbDeviceFilterPtr fastboot_device_filter_;

  // A mapping from GUID -> UsbDeviceInfo for each attached USB device
  std::map<std::string, device::mojom::UsbDeviceInfoPtr> available_device_info_;

  std::vector<SharedUsbDeviceInfo> shared_usb_devices_;

  base::ObserverList<SharedUsbDeviceObserver> shared_usb_device_observers_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<CrosUsbDetector> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CrosUsbDetector);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_USB_CROS_USB_DETECTOR_H_
