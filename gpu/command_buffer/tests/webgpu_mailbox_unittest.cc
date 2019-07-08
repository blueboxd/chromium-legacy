// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/client/webgpu_implementation.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/tests/webgpu_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"

using namespace testing;

namespace gpu {
namespace {

class MockBufferMapReadCallback {
 public:
  MOCK_METHOD4(Call,
               void(DawnBufferMapAsyncStatus status,
                    const uint32_t* ptr,
                    uint64_t data_length,
                    void* userdata));
};

std::unique_ptr<StrictMock<MockBufferMapReadCallback>>
    mock_buffer_map_read_callback;
void ToMockBufferMapReadCallback(DawnBufferMapAsyncStatus status,
                                 const void* ptr,
                                 uint64_t data_length,
                                 void* userdata) {
  // Assume the data is uint32_t
  mock_buffer_map_read_callback->Call(status, static_cast<const uint32_t*>(ptr),
                                      data_length, userdata);
}

class MockDeviceErrorCallback {
 public:
  MOCK_METHOD2(Call, void(const char* message, void* userdata));
};

std::unique_ptr<StrictMock<MockDeviceErrorCallback>> mock_device_error_callback;
void ToMockDeviceErrorCallback(const char* message, void* userdata) {
  mock_device_error_callback->Call(message, userdata);
}

}  // namespace

class WebGPUMailboxTest : public WebGPUTest {
 protected:
  void SetUp() override {
    WebGPUTest::SetUp();
    Initialize(WebGPUTest::Options());
    mock_buffer_map_read_callback =
        std::make_unique<StrictMock<MockBufferMapReadCallback>>();
    mock_device_error_callback =
        std::make_unique<StrictMock<MockDeviceErrorCallback>>();
  }

  void TearDown() override {
    mock_buffer_map_read_callback = nullptr;
    mock_device_error_callback = nullptr;
    WebGPUTest::TearDown();
  }
};

// Tests using Associate/DissociateMailbox to share an image with Dawn.
// For simplicity of the test the image is shared between a Dawn device and
// itself: we render to it using the Dawn device, then re-associate it to a
// Dawn texture and read back the values that were written.
TEST_F(WebGPUMailboxTest, WriteToMailboxThenReadFromIt) {
  if (!WebGPUSupported()) {
    LOG(ERROR) << "Test skipped because WebGPU isn't supported";
    return;
  }
  if (!WebGPUSharedImageSupported()) {
    LOG(ERROR) << "Test skipped because WebGPUSharedImage isn't supported";
    return;
  }

  // Create a the shared image
  SharedImageInterface* sii = GetSharedImageInterface();
  Mailbox mailbox = sii->CreateSharedImage(
      viz::ResourceFormat::RGBA_8888, {1, 1}, gfx::ColorSpace::CreateSRGB(),
      SHARED_IMAGE_USAGE_WEBGPU);
  SyncToken mailbox_produced_token = sii->GenVerifiedSyncToken();
  webgpu()->WaitSyncTokenCHROMIUM(mailbox_produced_token.GetConstData());

  dawn::Device device = dawn::Device::Acquire(webgpu()->GetDefaultDevice());

  // Part 1: Write to the texture using Dawn
  {
    // Register the shared image as a Dawn texture in the wire.
    gpu::webgpu::ReservedTexture reservation =
        webgpu()->ReserveTexture(device.Get());

    webgpu()->AssociateMailbox(0, 0, reservation.id, reservation.generation,
                               DAWN_TEXTURE_USAGE_BIT_OUTPUT_ATTACHMENT,
                               reinterpret_cast<GLbyte*>(&mailbox));
    dawn::Texture texture = dawn::Texture::Acquire(reservation.texture);

    // Clear the texture using a render pass.
    dawn::RenderPassColorAttachmentDescriptor color_desc;
    color_desc.attachment = texture.CreateDefaultView();
    color_desc.resolveTarget = nullptr;
    color_desc.loadOp = dawn::LoadOp::Clear;
    color_desc.storeOp = dawn::StoreOp::Store;
    color_desc.clearColor = {0, 255, 0, 255};

    dawn::RenderPassColorAttachmentDescriptor* color_attachments_ptr =
        &color_desc;

    dawn::RenderPassDescriptor render_pass_desc;
    render_pass_desc.colorAttachmentCount = 1;
    render_pass_desc.colorAttachments = &color_attachments_ptr;
    render_pass_desc.depthStencilAttachment = nullptr;

    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&render_pass_desc);
    pass.EndPass();
    dawn::CommandBuffer commands = encoder.Finish();

    dawn::Queue queue = device.CreateQueue();
    queue.Submit(1, &commands);

    // Dissociate the mailbox, flushing previous commands first
    webgpu()->FlushCommands();
    webgpu()->DissociateMailbox(reservation.id, reservation.generation);
  }

  // Part 2: Read back the texture using Dawn
  {
    // Register the shared image as a Dawn texture in the wire.
    gpu::webgpu::ReservedTexture reservation =
        webgpu()->ReserveTexture(device.Get());

    // Make sure previous Dawn wire commands are sent so that the texture IDs
    // are validated correctly.
    webgpu()->FlushCommands();

    webgpu()->AssociateMailbox(0, 0, reservation.id, reservation.generation,
                               DAWN_TEXTURE_USAGE_BIT_COPY_SRC,
                               reinterpret_cast<GLbyte*>(&mailbox));
    dawn::Texture texture = dawn::Texture::Acquire(reservation.texture);

    // Copy the texture in a mappable buffer.
    dawn::BufferDescriptor buffer_desc;
    buffer_desc.size = 4;
    buffer_desc.usage =
        dawn::BufferUsageBit::MapRead | dawn::BufferUsageBit::CopyDst;
    dawn::Buffer readback_buffer = device.CreateBuffer(&buffer_desc);

    dawn::TextureCopyView copy_src;
    copy_src.texture = texture;
    copy_src.mipLevel = 0;
    copy_src.arrayLayer = 0;
    copy_src.origin = {0, 0, 0};

    dawn::BufferCopyView copy_dst;
    copy_dst.buffer = readback_buffer;
    copy_dst.offset = 0;
    copy_dst.rowPitch = 256;
    copy_dst.imageHeight = 0;

    dawn::Extent3D copy_size = {1, 1, 1};

    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyTextureToBuffer(&copy_src, &copy_dst, &copy_size);
    dawn::CommandBuffer commands = encoder.Finish();

    dawn::Queue queue = device.CreateQueue();
    queue.Submit(1, &commands);

    // Dissociate the mailbox, flushing previous commands first
    webgpu()->FlushCommands();
    webgpu()->DissociateMailbox(reservation.id, reservation.generation);

    // Map the buffer and assert the pixel is the correct value.
    readback_buffer.MapReadAsync(ToMockBufferMapReadCallback, 0);
    uint32_t buffer_contents = 0xFF00FF00;
    EXPECT_CALL(*mock_buffer_map_read_callback,
                Call(DAWN_BUFFER_MAP_ASYNC_STATUS_SUCCESS,
                     Pointee(Eq(buffer_contents)), sizeof(uint32_t), 0))
        .Times(1);

    WaitForCompletion(device);
  }
}

// Tests that using a shared image aftr it is dissociated produces an error.
TEST_F(WebGPUMailboxTest, ErrorWhenUsingTextureAfterDissociate) {
  if (!WebGPUSupported()) {
    LOG(ERROR) << "Test skipped because WebGPU isn't supported";
    return;
  }
  if (!WebGPUSharedImageSupported()) {
    LOG(ERROR) << "Test skipped because WebGPUSharedImage isn't supported";
    return;
  }

  // Create a the shared image
  SharedImageInterface* sii = GetSharedImageInterface();
  Mailbox mailbox = sii->CreateSharedImage(
      viz::ResourceFormat::RGBA_8888, {1, 1}, gfx::ColorSpace::CreateSRGB(),
      SHARED_IMAGE_USAGE_WEBGPU);
  SyncToken mailbox_produced_token = sii->GenVerifiedSyncToken();
  webgpu()->WaitSyncTokenCHROMIUM(mailbox_produced_token.GetConstData());

  // Create the device, and expect a validation error.
  dawn::Device device = dawn::Device::Acquire(webgpu()->GetDefaultDevice());
  device.SetErrorCallback(ToMockDeviceErrorCallback, 0);

  // Associate and immediately dissociate the image.
  gpu::webgpu::ReservedTexture reservation =
      webgpu()->ReserveTexture(device.Get());
  dawn::Texture texture = dawn::Texture::Acquire(reservation.texture);

  webgpu()->AssociateMailbox(0, 0, reservation.id, reservation.generation,
                             DAWN_TEXTURE_USAGE_BIT_OUTPUT_ATTACHMENT,
                             reinterpret_cast<GLbyte*>(&mailbox));
  webgpu()->DissociateMailbox(reservation.id, reservation.generation);

  // Try using the texture, it should produce a validation error.
  dawn::TextureView view = texture.CreateDefaultView();
  EXPECT_CALL(*mock_device_error_callback, Call(_, _)).Times(1);
  WaitForCompletion(device);
}

}  // namespace gpu
