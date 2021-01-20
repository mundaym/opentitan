// Copyright lowRISC contributors.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#ifndef OPENTITAN_SW_DEVICE_LIB_RUNTIME_EPMP_H_
#define OPENTITAN_SW_DEVICE_LIB_RUNTIME_EPMP_H_

#include <stdint.h>

/**
 * RISC-V Enhanced Physical Memory Protection (EPMP).
 *
 * Specifications:
 *  - PMP Enhancements for memory access and execution prevention on Machine
 *    mode
 *    https://docs.google.com/document/d/1Mh_aiHYxemL0umN3GTTw8vsbmzHZ_nxZXgjgOUzbvc8
 *  - RISC-V Privileged Specfication (v. 20190608)
 *    https://github.com/riscv/riscv-isa-manual/releases/download/Ratified-IMFDQC-and-Priv-v1.11/riscv-privileged-20190608.pdf
 *  - Ibex PMP
 *    https://ibex-core.readthedocs.io/en/latest/03_reference/pmp.html
 *
 * Assumptions (should be initialized in assembly but can be verified using
 * `epmp_check`):
 *   * Rule Locking Bypass is enabled (mseccfg.RLB = 1)
 *   * Machine Mode Whitelist Policy is enabled (mseccfg.MMWP = 1)
 *   * Machine Mode Lockdown is disabled (mseccfg.MML = 0)
 */

/**
 * EPMP constants.
 *
 * TODO: there will probably be more of these.
 */
typedef enum epmp_constants {
  kEpmpGranularity = 0,
  kEpmpNumRegions = 16,
} epmp_constants_t;

/**
 * EPMP state.
 *
 * A copy of the EPMP state stored in RAM (or ROM). Call `epmp_set` to
 * update the EPMP configuration CSRs from a given `epmp_state_t`.
 */
typedef struct epmp_state {
  uint8_t pmpcfg[kEpmpNumRegions];
  uintptr_t pmpaddr[kEpmpNumRegions];
  // TODO: include mseccfg? creates ordering issues if value changes :(
} epmp_state_t;

/**
 * EPMP entry permissions.
 *
 * Unlocked permissions can generally be modified when in M-mode. Locked
 * permissions can only be modified in M-mode if Rule Locking Bypass
 * (mseccfg.RLB) is set.
 *
 * When Machine Mode Lockdown is disabled (mseccfg.MLL is unset) the
 * combination R=0 W=1 is reserved. Note that this is the assumed state
 * and so it is not possible to set these values.
 *
 * Note: these permissions have different meanings when Machine Mode
 * Lockdown (mseccfg.MLL) is set.
 *
 * TODO: MLL mode could be supported by adding additional enums. That
 * is why M-mode and S/U-mode permissions have been made explicit in
 * enum names.
 */
typedef enum epmp_permissions {
  // Unlocked permissions (can always be modified in machine mode).
  kEpmpPermissionsUnlockedMachineAllUserNone = 0,    /**< LRWX = 0b0000 */
  kEpmpPermissionsUnlockedMachineAllUserExecute,     /**< LRWX = 0b0001 */
  kEpmpPermissionsUnlockedMachineAllUserRead,        /**< LRWX = 0b0100 */
  kEpmpPermissionsUnlockedMachineAllUserReadExecute, /**< LRWX = 0b0101 */
  kEpmpPermissionsUnlockedMachineAllUserReadWrite,   /**< LRWX = 0b0110 */
  kEpmpPermissionsUnlockedMachineAllUserAll,         /**< LRWX = 0b0111 */

  // Locked permissions (can only be modified in machine mode when mseccfg.RLB
  // is set).
  kEpmpPermissionsLockedMachineNoneUserNone,               /**< LRWX = 0b1000 */
  kEpmpPermissionsLockedMachineExecuteUserExecute,         /**< LRWX = 0b1001 */
  kEpmpPermissionsLockedMachineReadUserRead,               /**< LRWX = 0b1100 */
  kEpmpPermissionsLockedMachineReadExecuteUserReadExecute, /**< LRWX = 0b1101 */
  kEpmpPermissionsLockedMachineReadWriteUserReadWrite,     /**< LRWX = 0b1110 */
  kEpmpPermissionsLockedMachineAllUserAll,                 /**< LRWX = 0b1111 */
} epmp_permissions_t;

/**
 * EPMP region specification.
 *
 * Provides the start and end addresses of a particular region. These addresses
 * are byte-aligned (i.e. they are like regular pointers rather than encoded
 * addresses).
 *
 * The intention is that this data structure is used to disambiguate the
 * addresses of regions. It is therefore recommended that `start` and `end` are
 * always used as labels when declaring a region.
 */
typedef struct epmp_region {
  uintptr_t start;
  uintptr_t end;
} epmp_region_t;

/**
 * EPMP entry index.
 *
 * Entries must be in the range [0, kEpmpNumRegions).
 */
typedef uint32_t epmp_entry_t;

/**
 * EPMP generic status codes.
 *
 * These error codes can be used by any function. However if a function
 * requires additional status codes, it must define a set of status codes to
 * be used exclusively by such function.
 */
typedef enum epmp_result {
  kEpmpOk = 0, /**< Success. */
  kEpmpError,  /**< General error. */
  kEpmpBadArg, /**< Input parameter is not valid. */
} epmp_result_t;

/**
 * EPMP configuration status codes.
 */
typedef enum epmp_entry_configure_result {
  kEpmpRegionConfigureOk = kEpmpOk,
  kEpmpRegionConfigureError = kEpmpError,
  kEpmpRegionConfigureBadArg = kEpmpBadArg,

  /**
   * Invalid addresses provided for the selected encoding method.
   *
   * TODO: is it useful to split this further? (e.g. alignment, size)
   */
  kEpmpRegionConfigureBadRegion,

  /**
   * Encoding the entry would interfere with a different pre-existing entry.
   *
   * New entries will be rejected if they:
   *  * Modify the base address of a pre-existing TOR entry.
   *  * Would result in an address being used in both a NAPOT/NA4 entry and a
   *    TOR entry.
   *
   * TODO: this means that TOR stacks need to be disabled top to bottom and then
   * re-added, is that too limiting?
   */
  kEpmpRegionConfigureConflict,
} epmp_entry_configure_result_t;

/**
 * Disable EPMP address matching.
 *
 * Address matching is disabled for `entry`. The pmpaddr for `entry` is set
 * to the value of `region.start` which must also match `region.end` (i.e.
 * the length of the region should be 0).
 *
 * Permissions are set as provided. It is expected that most users will set
 * the permissions as `kEpmpPermissionsUnlockedMachineAllUserNone` or, if
 * locking is required, `kEpmpPermissionsLockedMachineNoneUserNone`.
 *
 * Example (two TOR regions stacked + independent TOR region):
 *
 *   ...
 *   epmp_state_t state = {...};
 *   res0 = epmp_entry_configure_off(&state, 0,
 *              (epmp_region_t){0},
 *              kEpmpPermissionsUnlockedMachineAllUserNone);
 *   res1 = epmp_entry_configure_off(&state, 1,
 *              (epmp_region_t){ .start = 0x10, .end = 0x10 },
 *              kEpmpPermissionsLockedMachineNoneUserNone);
 *   ...
 *
 * Result:
 *
 *   Entry | Value of `pmpaddr` | Value of `pmpcfg` |
 *   ======+====================+===================+
 *       0 |   0x00 (0x00 >> 2) |         0b0000000 |
 *       1 |   0x04 (0x10 >> 2) |         0b1000000 |
 *
 * @param state State to update (unchanged on error).
 * @param entry Entry index to update (0 <= `entry` < `kEpmpNumRegions`)
 * @param region Updated value for pmpaddr (see above).
 * @param permissions Updated permissions to write to pmpcfg for `entry`.
 * @return `epmp_entry_configure_result_t`.
 */
epmp_entry_configure_result_t epmp_entry_configure_off(
    epmp_state_t *state, epmp_entry_t entry, epmp_region_t region,
    epmp_permissions_t permissions);

/**
 * Configures EPMP address matching to Top Of Range (TOR).
 *
 * Address matching is configured as TOR for the `entry` provided.
 *
 * The `region` end address will be written to the `pmpaddr` for `entry`.
 *
 * The `region` start address will be written to the `pmpaddr` for the
 * entry preceding `entry` if it is disabled (i.e. set to OFF). If
 * the preceding entry is set to TOR then the start address must match
 * the pre-existing `pmpaddr` for that entry (or 0 if `entry` is 0).
 * All other configurations will be rejected.
 *
 * Example (two TOR regions stacked + independent TOR region):
 *
 *   ...
 *   epmp_state_t state = {...};
 *   res0 = epmp_entry_configure_tor(&state, 0,
 *              (epmp_region_t){ .start = 0x00, .end = 0x10 },
 *              kEpmpPermissionsUnlockedMachineAllUserNone);
 *   res1 = epmp_entry_configure_tor(&state, 1,
 *              (epmp_region_t){ .start = 0x10, .end = 0x20 },
 *              kEpmpPermissionsUnlockedMachineAllUserAll);
 *   res2 = epmp_entry_configure_off(&state, 2,
 *              (epmp_region_t){ .start = 0x00, .end = 0x00 },
 *              kEpmpPermissionsUnlockedMachineNoneUserNone);
 *   res3 = epmp_entry_configure_tor(&state, 3,
 *              (epmp_region_t){ .start = 0x30, .end = 0x40 },
 *              kEpmpPermissionsUnlockedMachineAllUserAll);
 *   ...
 *
 * Result:
 *
 *   Entry | Value of `pmpaddr` | Value of `pmpcfg` |
 *   ======+====================+===================+
 *       0 |   0x04 (0x10 >> 2) |         0b0001000 |
 *       1 |   0x08 (0x20 >> 2) |         0b0001111 |
 *       2 |   0x0c (0x30 >> 2) |         0b0000000 |
 *       3 |   0x10 (0x40 >> 2) |         0b0001111 |
 *
 * @param state State to update (unchanged on error).
 * @param entry Entry index to update (0 <= `entry` < `kEpmpNumRegions`)
 * @param region Region start and end addresses. Start address must be 0
 *        for `entry` 0.
 * @param permissions Updated permissions to write to pmpcfg for `entry`.
 * @return `epmp_entry_configure_result_t`.
 */
epmp_entry_configure_result_t epmp_entry_configure_tor(
    epmp_state_t *state, epmp_entry_t entry, epmp_region_t region,
    epmp_permissions_t permissions);

/**
 * Configures EPMP address matching to Naturally Aligned four-byte (NA4).
 *
 * Address matching is configured as NA4 for `entry`. The `region.start`
 * address is written to `pmpaddr` for `entry`. The length of `region`
 * must be exactly four bytes.
 *
 * This function will return `kEpmpRegionConfigureBadRegion` if
 * `kEpmpGranularity` is greater than 0.
 *
 * Example:
 *
 *   ...
 *   epmp_state_t state = {...};
 *   res0 = epmp_entry_configure_na4(&state, 0,
 *              (epmp_region_t){ .start = 0x10, .end = 0x14 },
 *              kEpmpPermissionsUnlockedMachineAllUserAll);
 *   ...
 *
 * Result:
 *
 *   Entry | Value of `pmpaddr` | Value of `pmpcfg` |
 *   ======+====================+===================+
 *       0 |   0x04 (0x10 >> 2) |         0b0010111 |
 *
 * @param state State to update (unchanged on error).
 * @param entry Entry index to update (0 <= `entry` < `kEpmpNumRegions`)
 * @param region Region start and end addresses. Must be 4 byte aligned.
 * @param permissions Updated permissions to write to pmpcfg for `entry`.
 * @return `epmp_entry_configure_result_t`.
 */
epmp_entry_configure_result_t epmp_entry_configure_na4(
    epmp_state_t *state, epmp_entry_t entry, epmp_region_t region,
    epmp_permissions_t permissions);

/**
 * Configures EPMP address matching to Naturally Aligned Power-Of-Two (NAPOT).
 *
 * Address matching is configured as NAPOT for `entry`. The length of `region`
 * must be a power of two greater than four. The `region` (both start and end
 * addresses) must also be aligned to the same power of two.
 *
 * If `kEpmpGranularity` is greater than zero then the entire `region` must
 * also be aligned to `2 ** (2 + kEpmpGranularity)`.
 *
 * Example:
 *
 *   ...
 *   epmp_state_t state = {...};
 *   res0 = epmp_entry_configure_napot(&state, 0,
 *              (epmp_region_t){ .start = 0x10, .end = 0x20 },
 *              kEpmpPermissionsUnlockedMachineAllUserAll);
 *   res1 = epmp_entry_configure_napot(&state, 1,
 *              (epmp_region_t){ .start = 0x50, .end = 0x58 },
 *              kEpmpPermissionsUnlockedMachineAllUserNone);
 *   ...
 *
 * Result:
 *
 *   Entry | Value of `pmpaddr`        | Value of `pmpcfg` |
 *   ======+===========================+===================+
 *       0 | 0x41 ((0x10 >> 2) | 0b01) |         0b0011111 |
 *       1 | 0x14 ((0x50 >> 2) | 0b00) |         0b0011000 |
 *
 * @param state State to update (unchanged on error).
 * @param entry Entry index to update (0 <= `entry` < `kEpmpNumRegions`)
 * @param region Region start and end addresses.
 * @param permissions Updated permissions to write to pmpcfg for `entry`.
 * @return `epmp_entry_configure_result_t`.
 */
epmp_entry_configure_result_t epmp_entry_configure_napot(
    epmp_state_t *state, epmp_entry_t entry, epmp_region_t region,
    epmp_permissions_t permissions);

/**
 * Decode an entry from `state`.
 *
 * May access the preceding entry if `entry` is encoded using TOR.
 *
 * TODO: might be useful for debugging but not sure if we really need it.
 *
 * @param state State containing entry to decode.
 * @param entry Index of entry to decode (0 <= `entry` < `kEpmpNumRegions`)
 * @param[out] region Region start and end addresses (optional).
 * @param[out] permissions Permissions for the entry (optional).
 */
epmp_result_t epmp_entry_decode(const epmp_state_t *state, epmp_entry_t entry,
                                epmp_region_t *region,
                                epmp_permissions_t *permissions);

/**
 * EPMP set result.
 */
typedef enum epmp_set_result {
  kEpmpSetOk = kEpmpOk,
  kEpmpSetError = kEpmpError,
  kEpmpSetBadArg = kEpmpBadArg,

  /**
   * EPMP was not configured correctly resulting in a WARL (write any,
   * read legal) mismatch.
   */
  kEpmpSetMismatch,
} epmp_set_result_t;

/**
 * Update the EPMP configuration.
 *
 * Writes the values in `state` into the appropriate CSRs.
 *
 * @param state Desired register values.
 * @return `epmp_set_result_t`
 */
epmp_set_result_t epmp_set(const epmp_state_t *state);

/**
 * Read the current EPMP configuration.
 *
 * @param[out] state Destination to write register values to.
 * @return `epmp_result_t`
 */
epmp_result_t epmp_get(epmp_state_t *state);

/**
 * Check the current EPMP configuration against the expected state provided.
 *
 * Read the EPMP configuration from the relevant CSRs. Check the `pmpaddr` and
 * `pmpcfg` registers match those in `state`. Read the `mseccfg` register and
 * check that `mseccfg.RLB` = 1, `mseccfg.MLL` = 0 and `mseccfg.MMWP` = 1.
 *
 * TODO: make state optional (so you can just check mseccfg)?
 * TODO: make expected mseccfg value configurable?
 *
 * @param state Expected register values.
 * @return `epmp_result_t`
 */
epmp_result_t epmp_check(const epmp_state_t *state);

#endif  // OPENTITAN_SW_DEVICE_LIB_RUNTIME_EPMP_H_
