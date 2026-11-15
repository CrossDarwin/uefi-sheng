// SPDX-License-Identifier: BSD-3-Clause
// Host substitutes for UEFI services; the production policy and driver are compiled unchanged.
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#define STATIC static
#define IN
#define OUT
#define CONST const
#define VOID void
#define EFIAPI
#define TRUE 1
#define FALSE 0
#define DEBUG(args) ((void)0)
#define DEBUG_ERROR 1
#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))
#define MAX_UINT64 UINT64_MAX
#define MAX_UINTN SIZE_MAX
#define SIZE_4MB (4ULL<<20)
#define EFI_PAGE_SIZE 4096ULL
#define EFI_PAGES_TO_SIZE(x) ((x)*EFI_PAGE_SIZE)
#define EFI_SIZE_TO_PAGES(x) (((x)+4095)/4096)
#define ALIGN_VALUE(x,a) (((x)+(a)-1)&~((a)-1))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))
#define CopyMem memcpy
#define CompareMem memcmp
#define AsciiStrCmp strcmp
#define ReadUnaligned32(p) read32(p)
#define ReadUnaligned64(p) read64(p)
static uint32_t read32(const void*p) {uint32_t v;memcpy(&v,p,4);return v;}
static uint64_t read64(const void*p) {uint64_t v;memcpy(&v,p,8);return v;}
#define Fdt32ToCpu __builtin_bswap32
#define Fdt64ToCpu __builtin_bswap64
extern int fdt_check_header(const void*);
extern int fdt_path_offset(const void*,const char*);
extern const void *fdt_getprop(const void*,int,const char*,int*);
extern int fdt_first_subnode(const void*,int);
extern int fdt_next_subnode(const void*,int);
#define FdtCheckHeader fdt_check_header
#define FdtPathOffset fdt_path_offset
#define FdtGetProp fdt_getprop
#define FdtFirstSubnode fdt_first_subnode
#define FdtNextSubnode fdt_next_subnode
#define FdtTotalSize(p) __builtin_bswap32(read32((const char*)(p)+4))
typedef uint64_t UINT64;
typedef uint32_t UINT32;
typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef int32_t INT32;
typedef size_t UINTN;
typedef char CHAR8;
typedef int BOOLEAN;
typedef void *EFI_HANDLE;
typedef void EFI_SYSTEM_TABLE;
typedef uint64_t EFI_PHYSICAL_ADDRESS;
typedef int EFI_STATUS;
#define EFI_SUCCESS 0
#define EFI_DEVICE_ERROR 7
#define EFI_OUT_OF_RESOURCES 9
#define EFI_BUFFER_TOO_SMALL 5
#define EFI_ERROR(s) ((s)!=0)
#define EFI_MEMORY_UC 1
#define EFI_MEMORY_WC 2
#define EFI_MEMORY_WT 4
#define EFI_MEMORY_WB 8
#define EFI_MEMORY_XP 0x4000
#define AllocateAddress 2
#define NoHob 0
#define AddMem 1
#define AddDev 2
#define HobOnlyNoCacheSetting 3
#define SYS_MEM 0
#define MEM_RES 5
#define MMAP_IO 1
#define SYS_MEM_CAP 0x703c07
#define INITIALIZED 2
#define UNCACHEABLE 0x400
#define Conv 7
#define Reserv 0
#define BsData 4
#define RtData 6
#define MmIO 11
#define WRITE_BACK_XN 1
#define WRITE_BACK 1
#define WRITE_THROUGH_XN 2
#define UNCACHED_UNBUFFERED_XN 3
#define NS_DEVICE 4
#define EfiReservedMemoryType Reserv
#define EfiConventionalMemory Conv
#define MAX_ARM_MEMORY_REGION_DESCRIPTOR_COUNT 128
typedef struct {
 char Name[32]; uint64_t Address,Length;
 int HobOption,ResourceType;uint64_t ResourceAttribute;
 int MemoryType,ArmAttributes;
} EFI_MEMORY_REGION_DESCRIPTOR;
typedef struct {uint32_t Type;uint64_t PhysicalStart,VirtualStart,NumberOfPages,Attribute;} EFI_MEMORY_DESCRIPTOR;
#define EfiGcdMemoryTypeNonExistent 0
#define EfiGcdMemoryTypeReserved 1
#define EfiGcdMemoryTypeSystemMemory 2
typedef struct {uint64_t BaseAddress,Length,Capabilities,Attributes;int GcdMemoryType;void *ImageHandle,*DeviceHandle;} EFI_GCD_MEMORY_SPACE_DESCRIPTOR;
typedef struct {
 EFI_STATUS (*GetMemoryMap)(UINTN*,EFI_MEMORY_DESCRIPTOR*,UINTN*,UINTN*,UINT32*);
 EFI_STATUS (*AllocatePages)(int,int,UINTN,EFI_PHYSICAL_ADDRESS*);
} HOST_BOOT;
typedef struct {
 EFI_STATUS (*GetMemorySpaceMap)(UINTN*,EFI_GCD_MEMORY_SPACE_DESCRIPTOR**);
 EFI_STATUS (*RemoveMemorySpace)(UINT64,UINT64);
 EFI_STATUS (*AddMemorySpace)(int,UINT64,UINT64,UINT64);
 EFI_STATUS (*SetMemorySpaceAttributes)(UINT64,UINT64,UINT64);
} HOST_DXE;
extern HOST_BOOT *gBS;
extern HOST_DXE *gDS;
#define AllocatePool malloc
#define FreePool free
typedef struct {uint64_t Data;} EFI_HOB_GUID_TYPE;
extern int gSiliciumEntryArgHobGuid;
extern EFI_HOB_GUID_TYPE *GetFirstGuidHob(void*);
#define GET_GUID_HOB_DATA_SIZE(h) 8
#define GET_GUID_HOB_DATA(h) (&(h)->Data)
extern EFI_STATUS ArmSetMemoryAttributes(UINT64,UINT64,UINT64,UINT64);
