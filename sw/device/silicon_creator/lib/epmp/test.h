// Copyright lowRISC contributors.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#ifndef OPENTITAN_SW_DEVICE_SILICON_CREATOR_LIB_EPMP_TEST_H_
#define OPENTITAN_SW_DEVICE_SILICON_CREATOR_LIB_EPMP_TEST_H_

#include <stdbool.h>

#include "sw/device/silicon_creator/lib/epmp/epmp.h"

/**
 * Unlock the DV address space for read/write access.
 *
 * The production ePMP schema used by the mask ROM blocks all accesses to the DV
 * address space (a 4 byte window starting at `0x30000000`). The DV address
 * space is used by tests to report test progress and results and so must be
 * made accessible before tests can be run.
 *
 * Utilizes a dedicated PMP entry reserved for this usage.
 *
 * @param state Shadow register state to update and check against (optional).
 * @returns The result of the operation (`true` if address space unlocked
 * successfully).
 */
bool epmp_unlock_test_status(epmp_state_t *state);

#endif  // OPENTITAN_SW_DEVICE_SILICON_CREATOR_LIB_EPMP_TEST_H_
