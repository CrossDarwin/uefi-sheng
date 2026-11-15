/** @file
  Apply the qualified ABL memory map after the compatibility firmware's SEC.
**/

#include <Uefi.h>
#include <Library/ArmMmuLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/FdtLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/MemoryMapLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include "../../Library/MemoryMapLib/ShengMemoryMap.h"

STATIC
EFI_STATUS
ReserveHandoffMemory (
  EFI_MEMORY_REGION_DESCRIPTOR *Desired,
  UINT8 Count
  )
{
  EFI_MEMORY_DESCRIPTOR *Map;
  EFI_STATUS Status;
  UINTN Size = 0, Key, Stride;
  UINT32 Version;

  Status = gBS->GetMemoryMap (&Size, NULL, &Key, &Stride, &Version);
  if (Status != EFI_BUFFER_TOO_SMALL) { return EFI_DEVICE_ERROR; }
  // AllocatePool changes the map and may split several descriptors.
  if (Size > MAX_UINTN - 32 * sizeof (*Map)) { return EFI_OUT_OF_RESOURCES; }
  Size += 32 * sizeof (*Map);
  Map = AllocatePool (Size);
  if (Map == NULL) { return EFI_OUT_OF_RESOURCES; }
  Status = gBS->GetMemoryMap (&Size, Map, &Key, &Stride, &Version);
  if (EFI_ERROR (Status)) { FreePool (Map); return Status; }
  if (Stride < sizeof (*Map) || Size % Stride != 0) { FreePool (Map); return EFI_DEVICE_ERROR; }

  // Only take still-free pages. Loader, runtime and DXE allocations retain their owners.
  for (UINTN I = 0; I < Count; I++) {
    if (Desired[I].MemoryType != EfiReservedMemoryType) { continue; }
    for (UINTN At = 0; At < Size; At += Stride) {
      EFI_MEMORY_DESCRIPTOR *D = (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)Map + At);
      if (D->Type != EfiConventionalMemory ||
          D->NumberOfPages > (MAX_UINT64 - D->PhysicalStart) / EFI_PAGE_SIZE) { continue; }
      EFI_PHYSICAL_ADDRESS Base = MAX (Desired[I].Address, D->PhysicalStart);
      UINT64 End = MIN (Desired[I].Address + Desired[I].Length,
                        D->PhysicalStart + EFI_PAGES_TO_SIZE (D->NumberOfPages));
      if (Base >= End) { continue; }
      Status = gBS->AllocatePages (AllocateAddress, EfiReservedMemoryType,
                                  EFI_SIZE_TO_PAGES (End - Base), &Base);
      DEBUG ((DEBUG_ERROR, "SHENGMEM: retain 0x%lx + 0x%lx - %r\n", Base, End - Base, Status));
      if (EFI_ERROR (Status)) { FreePool (Map); return Status; }
    }
  }
  FreePool (Map);
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
PromoteMemory (
  EFI_PHYSICAL_ADDRESS Base,
  UINT64 Length,
  CONST EFI_GCD_MEMORY_SPACE_DESCRIPTOR *Previous
  )
{
  EFI_STATUS Status;
  BOOLEAN Removed = FALSE;
  UINT64 Capabilities = EFI_MEMORY_UC | EFI_MEMORY_WC | EFI_MEMORY_WT | EFI_MEMORY_WB;

  // The old SEC may not map an omitted bank fragment. Map it before DXE can allocate it.
  Status = ArmSetMemoryAttributes (Base, Length, EFI_MEMORY_WB | EFI_MEMORY_XP, 0);
  if (EFI_ERROR (Status)) { return Status; }
  if (Previous->GcdMemoryType == EfiGcdMemoryTypeReserved) {
    Status = gDS->RemoveMemorySpace (Base, Length);
    if (EFI_ERROR (Status)) { return Status; }
    Removed = TRUE;
  }
  Status = gDS->AddMemorySpace (EfiGcdMemoryTypeSystemMemory, Base, Length, Capabilities);
  if (EFI_ERROR (Status) && Removed) {
    // Restore the old reservation if publication failed; never publish on a failed map update.
    EFI_STATUS Restore = gDS->AddMemorySpace (EfiGcdMemoryTypeReserved, Base, Length, Previous->Capabilities);
    if (!EFI_ERROR (Restore)) {
      Restore = gDS->SetMemorySpaceAttributes (Base, Length, Previous->Attributes);
    }
    DEBUG ((DEBUG_ERROR, "SHENGMEM: restore reservation - %r\n", Restore));
  }
  return Status;
}

STATIC
VOID
AddMemory (CONST VOID *Fdt)
{
  EFI_MEMORY_REGION_DESCRIPTOR *Desired;
  EFI_GCD_MEMORY_SPACE_DESCRIPTOR *Map;
  UINT8 Count;
  UINTN MapCount;
  UINT64 Added = 0;
  EFI_STATUS Status;

  if (!ShengGetPreparedMemoryMap (Fdt, &Desired, &Count)) {
    DEBUG ((DEBUG_ERROR, "SHENGMEM: ABL map is not a qualified 8 GiB layout; unchanged\n"));
    return;
  }
  Status = ReserveHandoffMemory (Desired, Count);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "SHENGMEM: reservation update failed - %r\n", Status));
    return;
  }
  Status = gDS->GetMemorySpaceMap (&MapCount, &Map);
  if (EFI_ERROR (Status)) { return; }
  for (UINTN I = 0; I < Count; I++) {
    if (Desired[I].MemoryType != EfiConventionalMemory || Desired[I].ResourceType != SYS_MEM) { continue; }
    for (UINTN J = 0; J < MapCount; J++) {
      EFI_GCD_MEMORY_SPACE_DESCRIPTOR *D = &Map[J];
      if ((D->GcdMemoryType != EfiGcdMemoryTypeNonExistent && D->GcdMemoryType != EfiGcdMemoryTypeReserved) ||
          D->ImageHandle != NULL || D->DeviceHandle != NULL ||
          D->Length > MAX_UINT64 - D->BaseAddress) { continue; }
      UINT64 Base = MAX (Desired[I].Address, D->BaseAddress);
      UINT64 End = MIN (Desired[I].Address + Desired[I].Length, D->BaseAddress + D->Length);
      Base = ALIGN_VALUE (Base, EFI_PAGE_SIZE);
      End &= ~((UINT64)EFI_PAGE_SIZE - 1);
      if (Base >= End) { continue; }
      Status = PromoteMemory (Base, End - Base, D);
      DEBUG ((DEBUG_ERROR, "SHENGMEM: ABL RAM 0x%lx + 0x%lx - %r\n", Base, End - Base, Status));
      if (!EFI_ERROR (Status)) { Added += End - Base; }
    }
  }
  FreePool (Map);
  DEBUG ((DEBUG_ERROR, "SHENGMEM: qualified 8 GiB map - added 0x%lx bytes\n", Added));
}

EFI_STATUS
EFIAPI
ShengBootMemoryDxeEntry (IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_HOB_GUID_TYPE *Hob = GetFirstGuidHob (&gSiliciumEntryArgHobGuid);
  if (Hob == NULL || GET_GUID_HOB_DATA_SIZE (Hob) < sizeof (UINT64)) {
    DEBUG ((DEBUG_ERROR, "SHENGMEM: no entry argument HOB\n"));
    return EFI_SUCCESS;
  }
  UINT64 Dtb = ReadUnaligned64 (GET_GUID_HOB_DATA (Hob));
  if (Dtb == 0 || (Dtb & 3) != 0 || FdtCheckHeader ((VOID *)(UINTN)Dtb) != 0) {
    DEBUG ((DEBUG_ERROR, "SHENGMEM: invalid ABL device tree\n"));
    return EFI_SUCCESS;
  }
  AddMemory ((CONST VOID *)(UINTN)Dtb);
  return EFI_SUCCESS;
}
