// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/status_icons/status_icon_linux_dbus.h"

#include <dbus/dbus-shared.h>

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/numerics/checked_math.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/dbus/menu/menu.h"
#include "components/dbus/menu/properties_interface.h"
#include "components/dbus/menu/success_barrier_callback.h"
#include "components/dbus/menu/types.h"
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/linux_ui/status_icon_linux.h"

namespace {

// Service names.
const char kServiceStatusNotifierWatcher[] = "org.kde.StatusNotifierWatcher";

// Interfaces.
// If/when the StatusNotifierItem spec gets accepted AND widely used, replace
// "kde" with "freedesktop".
const char kInterfaceStatusNotifierItem[] = "org.kde.StatusNotifierItem";
const char kInterfaceStatusNotifierWatcher[] = "org.kde.StatusNotifierWatcher";

// Object paths.
const char kPathStatusNotifierItem[] = "/StatusNotifierItem";
const char kPathStatusNotifierWatcher[] = "/StatusNotifierWatcher";
const char kPathDbusMenu[] = "/com/canonical/dbusmenu";

// Methods.
const char kMethodNameHasOwner[] = "NameHasOwner";
const char kMethodRegisterStatusNotifierItem[] = "RegisterStatusNotifierItem";
const char kMethodActivate[] = "Activate";
const char kMethodContextMenu[] = "ContextMenu";
const char kMethodScroll[] = "Scroll";
const char kMethodSecondaryActivate[] = "SecondaryActivate";
const char kMethodGet[] = "Get";

// Properties.
const char kPropertyIsStatusNotifierHostRegistered[] =
    "IsStatusNotifierHostRegistered";
const char kPropertyItemIsMenu[] = "ItemIsMenu";
const char kPropertyWindowId[] = "WindowId";
const char kPropertyMenu[] = "Menu";
const char kPropertyAttentionIconName[] = "AttentionIconName";
const char kPropertyAttentionMovieName[] = "AttentionMovieName";
const char kPropertyCategory[] = "Category";
const char kPropertyIconName[] = "IconName";
const char kPropertyId[] = "Id";
const char kPropertyOverlayIconName[] = "OverlayIconName";
const char kPropertyStatus[] = "Status";
const char kPropertyTitle[] = "Title";
const char kPropertyAttentionIconPixmap[] = "AttentionIconPixmap";
const char kPropertyIconPixmap[] = "IconPixmap";
const char kPropertyOverlayIconPixmap[] = "OverlayIconPixmap";
const char kPropertyToolTip[] = "ToolTip";

// Signals.
const char kSignalNewIcon[] = "NewIcon";
const char kSignalNewToolTip[] = "NewToolTip";

// Property values.
const char kPropertyValueCategory[] = "ApplicationStatus";
const char kPropertyValueStatus[] = "Active";

scoped_refptr<dbus::Bus> CreateBus() {
  dbus::Bus::Options bus_options;
  bus_options.bus_type = dbus::Bus::SESSION;
  bus_options.connection_type = dbus::Bus::PRIVATE;
  bus_options.dbus_task_runner = dbus_thread_linux::GetTaskRunner();
  return base::MakeRefCounted<dbus::Bus>(bus_options);
}

int NextServiceId() {
  static int status_icon_count = 0;
  return ++status_icon_count;
}

std::string ServiceNameFromId(int service_id) {
  return std::string(kInterfaceStatusNotifierItem) + '-' +
         base::NumberToString(base::Process::Current().Pid()) + '-' +
         base::NumberToString(service_id);
}

std::string PropertyIdFromId(int service_id) {
  return "chrome_status_icon_" + base::NumberToString(service_id);
}

auto MakeDbusImage(const gfx::ImageSkia& image) {
  const SkBitmap* bitmap = image.bitmap();
  int width = bitmap->width();
  int height = bitmap->height();
  std::vector<uint8_t> color_data;
  auto size = base::CheckedNumeric<size_t>(4) * width * height;
  color_data.reserve(size.ValueOrDie());
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      SkColor color = bitmap->getColor(x, y);
      color_data.push_back(SkColorGetA(color));
      color_data.push_back(SkColorGetR(color));
      color_data.push_back(SkColorGetG(color));
      color_data.push_back(SkColorGetB(color));
    }
  }
  return MakeDbusArray(MakeDbusStruct(
      DbusInt32(width), DbusInt32(height),
      DbusByteArray(base::RefCountedBytes::TakeVector(&color_data))));
}

auto MakeDbusToolTip(const std::string& text) {
  return MakeDbusStruct(
      DbusString(""),
      DbusArray<DbusStruct<DbusInt32, DbusInt32, DbusByteArray>>(),
      DbusString(text), DbusString(""));
}

}  // namespace

StatusIconLinuxDbus::StatusIconLinuxDbus() : bus_(CreateBus()) {
  CheckStatusNotifierWatcherHasOwner();
}

StatusIconLinuxDbus::~StatusIconLinuxDbus() {
  bus_->GetDBusTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&dbus::Bus::ShutdownAndBlock, bus_));
}

void StatusIconLinuxDbus::SetIcon(const gfx::ImageSkia& image) {
  if (!properties_)
    return;

  properties_->SetProperty(kInterfaceStatusNotifierItem, kPropertyIconPixmap,
                           MakeDbusImage(image), true, false);
  dbus::Signal signal(kInterfaceStatusNotifierItem, kSignalNewIcon);
  item_->SendSignal(&signal);
}

void StatusIconLinuxDbus::SetToolTip(const base::string16& tool_tip) {
  if (!properties_)
    return;

  UpdateMenuImpl(delegate_->GetMenuModel(), true);

  properties_->SetProperty(
      kInterfaceStatusNotifierItem, kPropertyToolTip,
      MakeDbusToolTip(base::UTF16ToUTF8(delegate_->GetToolTip())));
  dbus::Signal signal(kInterfaceStatusNotifierItem, kSignalNewToolTip);
  item_->SendSignal(&signal);
}

void StatusIconLinuxDbus::UpdatePlatformContextMenu(ui::MenuModel* model) {
  UpdateMenuImpl(model, true);
}

void StatusIconLinuxDbus::RefreshPlatformContextMenu() {
  // This codepath gets called for property changes like changed labels or
  // icons, but also for layout changes like deleted items.
  // TODO(thomasanderson): Split this into two methods so we can avoid
  // rebuilding the menu for simple property changes.
  UpdateMenuImpl(delegate_->GetMenuModel(), true);
}

void StatusIconLinuxDbus::ExecuteCommand(int command_id, int event_flags) {
  DCHECK_EQ(command_id, 0);
  delegate_->OnClick();
}

void StatusIconLinuxDbus::CheckStatusNotifierWatcherHasOwner() {
  dbus::ObjectProxy* bus_proxy =
      bus_->GetObjectProxy(DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS));
  dbus::MethodCall method_call(DBUS_INTERFACE_DBUS, kMethodNameHasOwner);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(kServiceStatusNotifierWatcher);
  bus_proxy->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindRepeating(&StatusIconLinuxDbus::OnNameHasOwnerResponse,
                          weak_factory_.GetWeakPtr()));
}

void StatusIconLinuxDbus::OnNameHasOwnerResponse(dbus::Response* response) {
  dbus::MessageReader reader(response);
  bool owned = false;
  if (!response || !reader.PopBool(&owned) || !owned) {
    delegate_->OnImplInitializationFailed();
    return;
  }

  watcher_ = bus_->GetObjectProxy(kServiceStatusNotifierWatcher,
                                  dbus::ObjectPath(kPathStatusNotifierWatcher));

  dbus::MethodCall method_call(DBUS_INTERFACE_PROPERTIES, kMethodGet);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(kInterfaceStatusNotifierWatcher);
  writer.AppendString(kPropertyIsStatusNotifierHostRegistered);
  watcher_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindRepeating(&StatusIconLinuxDbus::OnHostRegisteredResponse,
                          weak_factory_.GetWeakPtr()));
}

void StatusIconLinuxDbus::OnHostRegisteredResponse(dbus::Response* response) {
  if (!response) {
    delegate_->OnImplInitializationFailed();
    return;
  }

  dbus::MessageReader reader(response);
  bool registered = false;
  if (!reader.PopVariantOfBool(&registered) || !registered) {
    delegate_->OnImplInitializationFailed();
    return;
  }

  service_id_ = NextServiceId();
  bus_->RequestOwnership(ServiceNameFromId(service_id_),
                         dbus::Bus::ServiceOwnershipOptions::REQUIRE_PRIMARY,
                         base::BindRepeating(&StatusIconLinuxDbus::OnOwnership,
                                             weak_factory_.GetWeakPtr()));
}

void StatusIconLinuxDbus::OnOwnership(const std::string& service_name,
                                      bool success) {
  if (!success) {
    delegate_->OnImplInitializationFailed();
    return;
  }

  static constexpr struct {
    const char* name;
    void (StatusIconLinuxDbus::*callback)(dbus::MethodCall*,
                                          dbus::ExportedObject::ResponseSender);
  } methods[4] = {
      {kMethodActivate, &StatusIconLinuxDbus::OnActivate},
      {kMethodContextMenu, &StatusIconLinuxDbus::OnContextMenu},
      {kMethodScroll, &StatusIconLinuxDbus::OnScroll},
      {kMethodSecondaryActivate, &StatusIconLinuxDbus::OnSecondaryActivate},
  };

  // The barrier requires base::size(methods) + 2 calls.  base::size(methods)
  // for each method exported, 1 for |properties_| initialization, and 1 for
  // |menu_| initialization.
  barrier_ =
      SuccessBarrierCallback(base::size(methods) + 2,
                             base::BindOnce(&StatusIconLinuxDbus::OnInitialized,
                                            weak_factory_.GetWeakPtr()));

  item_ = bus_->GetExportedObject(dbus::ObjectPath(kPathStatusNotifierItem));
  for (const auto& method : methods) {
    item_->ExportMethod(
        kInterfaceStatusNotifierItem, method.name,
        base::BindRepeating(method.callback, weak_factory_.GetWeakPtr()),
        base::BindRepeating(&StatusIconLinuxDbus::OnExported,
                            weak_factory_.GetWeakPtr()));
  }

  menu_ = std::make_unique<DbusMenu>(
      bus_->GetExportedObject(dbus::ObjectPath(kPathDbusMenu)), barrier_);
  UpdateMenuImpl(delegate_->GetMenuModel(), false);

  properties_ = std::make_unique<DbusPropertiesInterface>(item_, barrier_);
  properties_->RegisterInterface(kInterfaceStatusNotifierItem);
  auto set_property = [&](const std::string& property_name, auto&& value) {
    properties_->SetProperty(kInterfaceStatusNotifierItem, property_name,
                             std::move(value), false);
  };
  set_property(kPropertyItemIsMenu, DbusBoolean(false));
  set_property(kPropertyWindowId, DbusInt32(0));
  set_property(kPropertyMenu, DbusObjectPath(dbus::ObjectPath(kPathDbusMenu)));
  set_property(kPropertyAttentionIconName, DbusString(""));
  set_property(kPropertyAttentionMovieName, DbusString(""));
  set_property(kPropertyCategory, DbusString(kPropertyValueCategory));
  set_property(kPropertyIconName, DbusString(""));
  set_property(kPropertyId, DbusString(PropertyIdFromId(service_id_)));
  set_property(kPropertyOverlayIconName, DbusString(""));
  set_property(kPropertyStatus, DbusString(kPropertyValueStatus));
  set_property(kPropertyTitle, DbusString(""));
  set_property(kPropertyAttentionIconPixmap,
               DbusArray<DbusStruct<DbusInt32, DbusInt32, DbusByteArray>>());
  set_property(kPropertyIconPixmap, MakeDbusImage(delegate_->GetImage()));
  set_property(kPropertyOverlayIconPixmap,
               DbusArray<DbusStruct<DbusInt32, DbusInt32, DbusByteArray>>());
  set_property(kPropertyToolTip,
               MakeDbusToolTip(base::UTF16ToUTF8(delegate_->GetToolTip())));
}

void StatusIconLinuxDbus::OnExported(const std::string& interface_name,
                                     const std::string& method_name,
                                     bool success) {
  barrier_.Run(success);
}

void StatusIconLinuxDbus::OnInitialized(bool success) {
  if (!success) {
    delegate_->OnImplInitializationFailed();
    return;
  }

  dbus::MethodCall method_call(kInterfaceStatusNotifierWatcher,
                               kMethodRegisterStatusNotifierItem);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(ServiceNameFromId(service_id_));
  watcher_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::BindRepeating(&StatusIconLinuxDbus::OnRegistered,
                                           weak_factory_.GetWeakPtr()));
}

void StatusIconLinuxDbus::OnRegistered(dbus::Response* response) {
  if (!response)
    delegate_->OnImplInitializationFailed();
}

void StatusIconLinuxDbus::OnActivate(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender sender) {
  delegate_->OnClick();
  sender.Run(dbus::Response::FromMethodCall(method_call));
}

void StatusIconLinuxDbus::OnContextMenu(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender sender) {
  dbus::MessageReader reader(method_call);
  int32_t x;
  int32_t y;
  if (!reader.PopInt32(&x) || !reader.PopInt32(&y)) {
    sender.Run(nullptr);
    return;
  }

  if (!menu_runner_) {
    menu_runner_ = std::make_unique<views::MenuRunner>(
        concat_menu_.get(), views::MenuRunner::HAS_MNEMONICS |
                                views::MenuRunner::CONTEXT_MENU |
                                views::MenuRunner::FIXED_ANCHOR);
  }
  menu_runner_->RunMenuAt(
      nullptr, nullptr, gfx::Rect(gfx::Point(x, y), gfx::Size()),
      views::MenuAnchorPosition::kTopRight, ui::MENU_SOURCE_MOUSE);
  sender.Run(dbus::Response::FromMethodCall(method_call));
}

void StatusIconLinuxDbus::OnScroll(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender sender) {
  // Ignore scroll events.
  sender.Run(dbus::Response::FromMethodCall(method_call));
}

void StatusIconLinuxDbus::OnSecondaryActivate(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender sender) {
  // Intentionally ignore secondary activations.  In the future, we may decide
  // to run the same handler as regular activations.
  sender.Run(dbus::Response::FromMethodCall(method_call));
}

void StatusIconLinuxDbus::UpdateMenuImpl(ui::MenuModel* model,
                                         bool send_signal) {
  if (!menu_)
    return;

  if (!model) {
    empty_menu_ = std::make_unique<ui::SimpleMenuModel>(nullptr);
    model = empty_menu_.get();
  }

  click_action_menu_ = std::make_unique<ui::SimpleMenuModel>(this);
  if (delegate_->HasClickAction() && !delegate_->GetToolTip().empty()) {
    click_action_menu_->AddItem(0, delegate_->GetToolTip());
    if (model->GetItemCount())
      click_action_menu_->AddSeparator(ui::MenuSeparatorType::NORMAL_SEPARATOR);
  }

  concat_menu_ =
      std::make_unique<ConcatMenuModel>(click_action_menu_.get(), model);
  menu_->SetModel(concat_menu_.get(), send_signal);
  menu_runner_.reset();
}
