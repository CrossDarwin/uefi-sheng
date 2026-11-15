// SPDX-License-Identifier: BSD-3-Clause
#ifndef SHENG_MEMORY_MAP_H
#define SHENG_MEMORY_MAP_H

// SEC calls this before constructing the MMU tables and resource HOBs.
VOID ShengPrepareMemoryMap (IN CONST VOID *Fdt);

// DXE must reject an unqualified FDT instead of using the SEC fallback map.
BOOLEAN ShengGetPreparedMemoryMap (
  IN CONST VOID *Fdt,
  OUT EFI_MEMORY_REGION_DESCRIPTOR **Descriptors,
  OUT UINT8 *Count
  );

#endif
