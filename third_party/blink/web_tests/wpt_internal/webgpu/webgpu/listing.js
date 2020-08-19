// AUTO-GENERATED - DO NOT EDIT. See src/common/tools/gen_listings.ts.

export const listing = [
  {
    "file": [],
    "readme": "WebGPU conformance test suite."
  },
  {
    "file": [
      "api"
    ],
    "readme": "Tests for full coverage of the Javascript API surface of WebGPU."
  },
  {
    "file": [
      "api",
      "operation"
    ],
    "readme": "Tests that check the result of performing valid WebGPU operations, taking advantage of\nparameterization to exercise interactions between features."
  },
  {
    "file": [
      "api",
      "operation",
      "buffers"
    ],
    "readme": "GPUBuffer tests."
  },
  {
    "file": [
      "api",
      "operation",
      "buffers",
      "map"
    ],
    "description": ""
  },
  {
    "file": [
      "api",
      "operation",
      "buffers",
      "map_detach"
    ],
    "description": ""
  },
  {
    "file": [
      "api",
      "operation",
      "buffers",
      "map_oom"
    ],
    "description": ""
  },
  {
    "file": [
      "api",
      "operation",
      "command_buffer",
      "basic"
    ],
    "description": "Basic tests."
  },
  {
    "file": [
      "api",
      "operation",
      "command_buffer",
      "compute",
      "basic"
    ],
    "description": "Basic command buffer compute tests."
  },
  {
    "file": [
      "api",
      "operation",
      "command_buffer",
      "copies"
    ],
    "description": "copy{Buffer,Texture}To{Buffer,Texture} tests."
  },
  {
    "file": [
      "api",
      "operation",
      "command_buffer",
      "render",
      "basic"
    ],
    "description": "Basic command buffer rendering tests."
  },
  {
    "file": [
      "api",
      "operation",
      "command_buffer",
      "render",
      "rendering"
    ],
    "description": ""
  },
  {
    "file": [
      "api",
      "operation",
      "command_buffer",
      "render",
      "storeop"
    ],
    "description": "renderPass store op test that drawn quad is either stored or cleared based on storeop"
  },
  {
    "file": [
      "api",
      "operation",
      "fences"
    ],
    "description": ""
  },
  {
    "file": [
      "api",
      "operation",
      "render_pass",
      "storeOp"
    ],
    "description": "API Operation Tests for RenderPass StoreOp.\n\n  Test Coverage:\n\n  - Tests that color and depth-stencil store operations {'clear', 'store'} work correctly for a\n    render pass with both a color attachment and depth-stencil attachment.\n      TODO: use depth24plus-stencil8\n\n  - Tests that store operations {'clear', 'store'} work correctly for a render pass with multiple\n    color attachments.\n      TODO: test with more interesting loadOp values\n\n  - Tests that store operations {'clear', 'store'} work correctly for a render pass with a color\n    attachment for:\n      - All renderable color formats\n      - mip level set to {'0', mip > '0'}\n      - array layer set to {'0', layer > '1'} for 2D textures\n      TODO: depth slice set to {'0', slice > '0'} for 3D textures\n\n  - Tests that store operations {'clear', 'store'} work correctly for a render pass with a\n    depth-stencil attachment for:\n      - All renderable depth-stencil formats\n      - mip level set to {'0', mip > '0'}\n      - array layer set to {'0', layer > '1'} for 2D textures\n      TODO: test depth24plus and depth24plus-stencil8 formats\n      TODO: test that depth and stencil aspects are set seperately\n      TODO: depth slice set to {'0', slice > '0'} for 3D textures\n      TODO: test with more interesting loadOp values"
  },
  {
    "file": [
      "api",
      "operation",
      "render_pipeline",
      "culling_tests"
    ],
    "description": "Test culling and rasterizaion state.\n\nTest coverage:\nTest all culling combinations of GPUFrontFace and GPUCullMode show the correct output.\n\nUse 2 triangles with different winding orders:\n\n- Test that the counter-clock wise triangle has correct output for:\n  - All FrontFaces (ccw, cw)\n  - All CullModes (none, front, back)\n  - All depth stencil attachment types (none, depth24plus, depth32float, depth24plus-stencil8)\n  - Some primitive topologies (triangle-list, TODO: triangle-strip)\n\n- Test that the clock wise triangle has correct output for:\n  - All FrontFaces (ccw, cw)\n  - All CullModes (none, front, back)\n  - All depth stencil attachment types (none, depth24plus, depth32float, depth24plus-stencil8)\n  - Some primitive topologies (triangle-list, TODO: triangle-strip)"
  },
  {
    "file": [
      "api",
      "operation",
      "resource_init",
      "copied_texture_clear"
    ],
    "description": "Test uninitialized textures are initialized to zero when copied."
  },
  {
    "file": [
      "api",
      "operation",
      "resource_init",
      "depth_stencil_attachment_clear"
    ],
    "description": "Test uninitialized textures are initialized to zero when used as a depth/stencil attachment."
  },
  {
    "file": [
      "api",
      "operation",
      "resource_init",
      "sampled_texture_clear"
    ],
    "description": "Test uninitialized textures are initialized to zero when sampled."
  },
  {
    "file": [
      "api",
      "regression"
    ],
    "readme": "One-off tests that reproduce API bugs found in implementations to prevent the bugs from\nappearing again."
  },
  {
    "file": [
      "api",
      "validation"
    ],
    "readme": "Positive and negative tests for all the validation rules of the API."
  },
  {
    "file": [
      "api",
      "validation",
      "copyBufferToBuffer"
    ],
    "description": "copyBufferToBuffer tests.\n\nTest Plan:\n* Buffer is valid/invalid\n  - the source buffer is invalid\n  - the destination buffer is invalid\n* Buffer usages\n  - the source buffer is created without GPUBufferUsage::COPY_SRC\n  - the destination buffer is created without GPUBufferUsage::COPY_DEST\n* CopySize\n  - copySize is not a multiple of 4\n  - copySize is 0\n* copy offsets\n  - sourceOffset is not a multiple of 4\n  - destinationOffset is not a multiple of 4\n* Arthimetic overflow\n  - (sourceOffset + copySize) is overflow\n  - (destinationOffset + copySize) is overflow\n* Out of bounds\n  - (sourceOffset + copySize) > size of source buffer\n  - (destinationOffset + copySize) > size of destination buffer\n* Source buffer and destination buffer are the same buffer"
  },
  {
    "file": [
      "api",
      "validation",
      "copy_between_linear_data_and_texture"
    ],
    "readme": "writeTexture + copyBufferToTexture + copyTextureToBuffer validation tests.\n\nTest coverage:\n* resource usages:\n\t- texture_usage_must_be_valid: for GPUTextureUsage::COPY_SRC, GPUTextureUsage::COPY_DST flags.\n\n* textureCopyView:\n\t- texture_must_be_valid: for valid, destroyed, error textures.\n\t- sample_count_must_be_1: for sample count 1 and 4.\n\t- mip_level_must_be_in_range: for various combinations of mipLevel and mipLevelCount.\n\t- texel_block_alignment_on_origin: for all formats and coordinates.\n\n* linear texture data:\n\t- bound_on_rows_per_image: for various combinations of copyDepth (1, >1), copyHeight, rowsPerImage.\n\t- offset_plus_required_bytes_in_copy_overflow\n\t- required_bytes_in_copy: testing minimal data size and data size too small for various combinations of bytesPerRow, rowsPerImage, copyExtent and offset. for the copy method, bytesPerRow is computed as bytesInACompleteRow aligned to be a multiple of 256 + bytesPerRowPadding * 256.\n\t- texel_block_alignment_on_rows_per_image: for all formats.\n\t- texel_block_alignment_on_offset: for all formats.\n\t- bound_on_bytes_per_row: for all formats and various combinations of bytesPerRow and copyExtent. for writeTexture, bytesPerRow is computed as (blocksPerRow * blockWidth * bytesPerBlock + additionalBytesPerRow) and copyExtent.width is computed as copyWidthInBlocks * blockWidth. for the copy methods, both values are mutliplied by 256.\n\t- bound_on_offset: for various combinations of offset and dataSize.\n\n* texture copy range:\n\t- 1d_texture: copyExtent.height isn't 1, copyExtent.depth isn't 1.\n\t- texel_block_alignment_on_size: for all formats and coordinates.\n\t- texture_range_conditons: for all coordinate and various combinations of origin, copyExtent, textureSize and mipLevel.\n\nTODO: more test coverage for 1D and 3D textures."
  },
  {
    "file": [
      "api",
      "validation",
      "copy_between_linear_data_and_texture",
      "copyBetweenLinearDataAndTexture_dataRelated"
    ],
    "description": ""
  },
  {
    "file": [
      "api",
      "validation",
      "copy_between_linear_data_and_texture",
      "copyBetweenLinearDataAndTexture_textureRelated"
    ],
    "description": ""
  },
  {
    "file": [
      "api",
      "validation",
      "createBindGroup"
    ],
    "description": "createBindGroup validation tests."
  },
  {
    "file": [
      "api",
      "validation",
      "createBindGroupLayout"
    ],
    "description": "createBindGroupLayout validation tests."
  },
  {
    "file": [
      "api",
      "validation",
      "createPipelineLayout"
    ],
    "description": "createPipelineLayout validation tests."
  },
  {
    "file": [
      "api",
      "validation",
      "createRenderPipeline"
    ],
    "description": "createRenderPipeline validation tests."
  },
  {
    "file": [
      "api",
      "validation",
      "createTexture"
    ],
    "description": "createTexture validation tests."
  },
  {
    "file": [
      "api",
      "validation",
      "createView"
    ],
    "description": "createView validation tests."
  },
  {
    "file": [
      "api",
      "validation",
      "encoding",
      "cmds",
      "index_access"
    ],
    "description": "indexed draws validation tests."
  },
  {
    "file": [
      "api",
      "validation",
      "error_scope"
    ],
    "description": "error scope validation tests."
  },
  {
    "file": [
      "api",
      "validation",
      "fences"
    ],
    "description": "fences validation tests."
  },
  {
    "file": [
      "api",
      "validation",
      "queue_submit"
    ],
    "description": "queue submit validation tests."
  },
  {
    "file": [
      "api",
      "validation",
      "render_pass"
    ],
    "description": "render pass validation tests."
  },
  {
    "file": [
      "api",
      "validation",
      "render_pass",
      "resolve"
    ],
    "description": "API Validation Tests for RenderPass Resolve.\n\n  Test Coverage:\n    - When resolveTarget is not null:\n      - Test that the colorAttachment is multisampled:\n        - A single sampled colorAttachment should generate an error.\n      - Test that the resolveTarget is single sampled:\n        - A multisampled resolveTarget should generate an error.\n      - Test that the resolveTarget has usage OUTPUT_ATTACHMENT:\n        - A resolveTarget without usage OUTPUT_ATTACHMENT should generate an error.\n      - Test that the resolveTarget's texture view describes a single subresource:\n        - A resolveTarget texture view with base mip {0, base mip > 0} and mip count of 1 should be\n          valid.\n          - An error should be generated when the resolve target view mip count is not 1 and base\n            mip is {0, base mip > 0}.\n        - A resolveTarget texture view with base array layer {0, base array layer > 0} and array\n          layer count of 1 should be valid.\n          - An error should be generated when the resolve target view array layer count is not 1 and\n            base array layer is {0, base array layer > 0}.\n      - Test that the resolveTarget's format is the same as the colorAttachment:\n        - An error should be generated when the resolveTarget's format does not match the\n          colorAttachment's format.\n      - Test that the resolveTarget's size is the same the colorAttachment:\n        - An error should be generated when the resolveTarget's height or width are not equal to\n          the colorAttachment's height or width."
  },
  {
    "file": [
      "api",
      "validation",
      "render_pass",
      "storeOp"
    ],
    "description": "API Validation Tests for RenderPass StoreOp.\n\nTest Coverage:\n  - Tests that when depthReadOnly is true, depthStoreOp must be 'store'.\n    - When depthReadOnly is true and depthStoreOp is 'clear', an error should be generated.\n\n  - Tests that when stencilReadOnly is true, stencilStoreOp must be 'store'.\n    - When stencilReadOnly is true and stencilStoreOp is 'clear', an error should be generated.\n\n  - Tests that the depthReadOnly value matches the stencilReadOnly value.\n    - When depthReadOnly does not match stencilReadOnly, an error should be generated.\n\n  - Tests that depthReadOnly and stencilReadOnly default to false."
  },
  {
    "file": [
      "api",
      "validation",
      "render_pass_descriptor"
    ],
    "description": "render pass descriptor validation tests."
  },
  {
    "file": [
      "api",
      "validation",
      "resource_usages",
      "textureUsageInRender"
    ],
    "description": "Texture Usages Validation Tests in Render Pass.\n\nTest Coverage:\n - Tests that read and write usages upon the same texture subresource, or different subresources\n   of the same texture. Different subresources of the same texture includes different mip levels,\n   different array layers, and different aspects.\n   - When read and write usages are binding to the same texture subresource, an error should be\n     generated. Otherwise, no error should be generated."
  },
  {
    "file": [
      "api",
      "validation",
      "setBindGroup"
    ],
    "description": "setBindGroup validation tests."
  },
  {
    "file": [
      "api",
      "validation",
      "setBlendColor"
    ],
    "description": "setBlendColor validation tests."
  },
  {
    "file": [
      "api",
      "validation",
      "setScissorRect"
    ],
    "description": "setScissorRect validation tests."
  },
  {
    "file": [
      "api",
      "validation",
      "setStencilReference"
    ],
    "description": "setStencilReference validation tests."
  },
  {
    "file": [
      "api",
      "validation",
      "setVertexBuffer"
    ],
    "description": "setVertexBuffer validation tests."
  },
  {
    "file": [
      "api",
      "validation",
      "setViewport"
    ],
    "description": "setViewport validation tests."
  },
  {
    "file": [
      "api",
      "validation",
      "vertex_state"
    ],
    "description": "vertexState validation tests."
  },
  {
    "file": [
      "examples"
    ],
    "description": "Examples of writing CTS tests with various features.\n\nStart here when looking for examples of basic framework usage."
  },
  {
    "file": [
      "idl"
    ],
    "readme": "Tests to check that the WebGPU IDL is correctly implemented, for examples that objects exposed\nexactly the correct members, and that methods throw when passed incomplete dictionaries."
  },
  {
    "file": [
      "idl",
      "constants",
      "flags"
    ],
    "description": "Test the values of flags interfaces (e.g. GPUTextureUsage)."
  },
  {
    "file": [
      "shader"
    ],
    "readme": "Tests for full coverage of the shaders that can be passed to WebGPU."
  },
  {
    "file": [
      "shader",
      "execution"
    ],
    "readme": "Tests that check the result of valid shader execution."
  },
  {
    "file": [
      "shader",
      "execution",
      "robust_access"
    ],
    "description": "Tests to check array clamping in shaders is correctly implemented including vector / matrix indexing"
  },
  {
    "file": [
      "shader",
      "execution",
      "robust_access_vertex"
    ],
    "description": "Test vertex attributes behave correctly (no crash / data leak) when accessed out of bounds\n\nTest coverage:\n\nThe following will be parameterized (all combinations tested):\n\n1) Draw call indexed? (false / true)\n  - Run the draw call using an index buffer\n\n2) Draw call indirect? (false / true)\n  - Run the draw call using an indirect buffer\n\n3) Draw call parameter (vertexCount, firstVertex, indexCount, firstIndex, baseVertex, instanceCount,\n  firstInstance)\n  - The parameter which will go out of bounds. Filtered depending on if the draw call is indexed.\n\n4) Attribute type (float, vec2, vec3, vec4)\n  - The input attribute type in the vertex shader\n\n5) Error scale (1, 4, 10^2, 10^4, 10^6)\n  - Offset to add to the correct draw call parameter\n\n6) Additional vertex buffers (0, +4)\n  - Tests that no OOB occurs if more vertex buffers are used\n\nThe tests will also have another vertex buffer bound for an instanced attribute, to make sure\ninstanceCount / firstInstance are tested.\n\nThe tests will include multiple attributes per vertex buffer.\n\nThe vertex buffers will be filled by repeating a few chosen values until the end of the buffer.\n\nThe test will run a render pipeline which verifies the following:\n1) All vertex attribute values occur in the buffer or are zero\n2) All gl_VertexIndex values are within the index buffer or 0\n\nTODO:\n\nA suppression may be needed for d3d12 on tests that have non-zero baseVertex, since d3d12 counts\nfrom 0 instead of from baseVertex (will fail check for gl_VertexIndex).\n\nVertex buffer contents could be randomized to prevent the case where a previous test creates\na similar buffer to ours and the OOB-read seems valid. This should be deterministic, which adds\nmore complexity that we may not need."
  },
  {
    "file": [
      "shader",
      "regression"
    ],
    "readme": "One-off tests that reproduce shader bugs found in implementations to prevent the bugs from\nappearing again."
  },
  {
    "file": [
      "shader",
      "validation"
    ],
    "readme": "Positive and negative tests for all the validation rules of the shading language."
  },
  {
    "file": [
      "web-platform"
    ],
    "readme": "Tests for Web platform-specific interactions like GPUSwapChain and canvas, WebXR,\nImageBitmaps, and video APIs."
  },
  {
    "file": [
      "web-platform",
      "canvas",
      "context_creation"
    ],
    "description": ""
  },
  {
    "file": [
      "web-platform",
      "copyImageBitmapToTexture"
    ],
    "description": "copy imageBitmap To texture tests."
  }
];
