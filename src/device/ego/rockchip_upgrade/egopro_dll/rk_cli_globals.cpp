// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.
//
// Definitions for globals that the Rockchip upgrade-tool sources declare
// `extern` but define only in their CLI front-end (main.cpp), which is
// intentionally excluded from this build (see the sibling CMakeLists.txt).
// Providing them here keeps the CLI out of the build while satisfying the
// linker.
//
// g_bCBW_parity toggles the optional CBW parity field in CRKUsbComm::RKU_WriteLBA.
// The upstream tool leaves it false unless a command-line option enables it;
// false preserves the default (no-parity) bulk-transfer behaviour.
bool g_bCBW_parity = false;
