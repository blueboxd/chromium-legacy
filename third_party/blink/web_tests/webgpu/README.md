To roll the WebGPU CTS forward to the latest top-of-tree, use
`roll_webgpu_cts.sh`. This does not regenerate `wpt_internal/webgpu/cts.html`.

To regenerate `wpt_internal/webgpu/cts.html` (after a roll, or after changes
in WebGPUExpectations), run `regenerate_internal_cts_html.sh`.

See the comments in those scripts for more info.
