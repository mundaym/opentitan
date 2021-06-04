# Copyright lowRISC contributors.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

# Silicon creator ePMP module
sw_silicon_creator_lib_epmp = declare_dependency(
  link_with: static_library(
    'sw_silicon_creator_lib_epmp',
    sources: [
      'epmp.c',
    ],
  ),
)

# Silicon creator ePMP test utilities
sw_silicon_creator_lib_epmp_test = declare_dependency(
  link_with: static_library(
    'sw_silicon_creator_lib_epmp_test',
    sources: [
      'test.c',
    ],
    dependencies: [
      sw_silicon_creator_lib_epmp,
    ],
  ),
)

test('sw_silicon_creator_lib_epmp_unittest', executable(
    'sw_silicon_creator_lib_epmp_unittest',
    sources: [
      'epmp_unittest.cc',
      'epmp.c',
    ],
    dependencies: [
      sw_vendor_gtest,
      sw_lib_base_testing_mock_csr,
      sw_lib_testing_bitfield,
    ],
    native: true,
    c_args: ['-DMOCK_CSR'],
    cpp_args: ['-DMOCK_CSR'],
    ),
  suite: 'mask_rom',
)
