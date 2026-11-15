// SPDX-License-Identifier: BSD-3-Clause
#include "Host.h"
#include "../../Platforms/Xiaomi/shengPkg/Library/MemoryMapLib/MemoryMapLib.c"
#include "../../Platforms/Xiaomi/shengPkg/Drivers/ShengBootMemoryDxe/ShengBootMemoryDxe.c"

static int Adds, Maps, Retained, FailMap, FailReserve, High, Removed;
static EFI_MEMORY_REGION_DESCRIPTOR *Plan;
static UINT8 PlanCount;
int gSiliciumEntryArgHobGuid;
EFI_HOB_GUID_TYPE *GetFirstGuidHob(void*p) {(void)p; return NULL;}
static int inside_free(uint64_t base,uint64_t end) {
 for(unsigned i=0;i<PlanCount;i++)
  if(Plan[i].MemoryType==Conv && Plan[i].ResourceType==SYS_MEM && base>=Plan[i].Address && end<=Plan[i].Address+Plan[i].Length) return 1;
 return 0;
}
static EFI_STATUS bootmap(UINTN *size,EFI_MEMORY_DESCRIPTOR *map,UINTN *key,UINTN *stride,UINT32 *version) {
 EFI_MEMORY_DESCRIPTOR entries[]={{.Type=Conv,.PhysicalStart=0xe671d000,.NumberOfPages=(0xff800000-0xe671d000)/4096},
                                {.Type=Conv,.PhysicalStart=0x8d0000000,.NumberOfPages=512}};
 *key=1;*stride=sizeof(entries[0]);*version=1;
 if(!map || *size<sizeof(entries)) {*size=sizeof(entries);return EFI_BUFFER_TOO_SMALL;}
 memcpy(map,entries,sizeof(entries));*size=sizeof(entries);return 0;
}
static EFI_STATUS reserve(int kind,int type,UINTN pages,UINT64 *base) {
 assert(kind==AllocateAddress && type==Reserv && pages);
 assert(!inside_free(*base,*base+pages*4096));Retained++;return FailReserve?7:0;
}
static EFI_STATUS gcdmap(UINTN *count,EFI_GCD_MEMORY_SPACE_DESCRIPTOR **out) {
 EFI_GCD_MEMORY_SPACE_DESCRIPTOR rows[]={
  {0x80000000,0x2000000,0,0,1,NULL,NULL},{0x82000000,0x100000,0,0,1,(void*)1,NULL},
  {0x82100000,0x7df00000,0,0,1,NULL,NULL},
  {0x880000000,0x3b300000,0,0,2,NULL,NULL},{0x8bb300000,0x200000,0,0,0,NULL,NULL},
  {0x8bb500000,0x1e00000,0,0,0,NULL,NULL},{0x8bd300000,0x2d00000,0,0,0,NULL,NULL},
  {0x8c0000000,0x140000000,0,0,2,NULL,NULL}};
 *count=ARRAY_SIZE(rows);*out=malloc(sizeof(rows));memcpy(*out,rows,sizeof(rows));return 0;
}
static EFI_STATUS remove_range(UINT64 base,UINT64 len) {
 assert(inside_free(base,base+len));Removed++;return 0;
}
static EFI_STATUS publish(int kind,UINT64 base,UINT64 len,UINT64 attrs) {
 assert(kind==2 && attrs&EFI_MEMORY_WB && Maps>Adds);
 assert(inside_free(base,base+len));
 assert(base+len<=0x82000000 || base>=0x82100000);
 assert(base+len<=0x8bb300000 || base>=0x8bb500000);
 if(base==0x8bb500000 && len==0x1e00000) High++;
 Adds++;return 0;
}
static EFI_STATUS attrs(UINT64 base,UINT64 len,UINT64 a) {(void)base;(void)len;(void)a;return 0;}
EFI_STATUS ArmSetMemoryAttributes(UINT64 base,UINT64 len,UINT64 a,UINT64 mask) {
 assert(inside_free(base,base+len) && a==(EFI_MEMORY_WB|EFI_MEMORY_XP) && !mask);Maps++;return FailMap?7:0;
}
static HOST_BOOT bs={bootmap,reserve};HOST_BOOT *gBS=&bs;
static HOST_DXE ds={gcdmap,remove_range,publish,attrs};HOST_DXE *gDS=&ds;
int main(int argc,char **argv) {
 assert(argc==2);
 int fd=open(argv[1],O_RDONLY);assert(fd>=0);struct stat st;assert(!fstat(fd,&st));
 void *fdt=mmap((void*)0x90000000,0x100000,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
 assert(fdt!=MAP_FAILED);assert(read(fd,fdt,st.st_size)==st.st_size);close(fd);
 assert(ShengGetPreparedMemoryMap(fdt,&Plan,&PlanCount));
 assert(PlanCount>0 && !inside_free(0xd0000000,0xd0010000));
 assert(!inside_free(0x90000000,0x90001000) && !inside_free(0x8d0000000,0x8d0200000));
 assert(!inside_free(0xe671d000,0xe7cb0000));
 AddMemory(fdt);assert(Adds>0 && High==1 && Retained>0 && Removed>0);
 Adds=Maps=Removed=0;FailMap=1;AddMemory(fdt);assert(Maps>0 && Adds==0 && Removed==0);
 FailMap=0;FailReserve=1;Adds=Maps=0;AddMemory(fdt);assert(!Adds && !Maps);FailReserve=0;
 // A duplicated bank must not publish any memory, even when its addresses look valid.
 int bytes=0;void *reg=(void*)FdtGetProp(fdt,FdtPathOffset(fdt,"/memory"),"reg",&bytes);assert(bytes>=32);
 unsigned char saved[16];memcpy(saved,(char*)reg+16,16);memcpy((char*)reg+16,reg,16);
 Adds=Maps=0;AddMemory(fdt);assert(!Adds && !Maps);memcpy((char*)reg+16,saved,16);
 // Reject an overflowing bank size before updating either map.
 uint64_t size=UINT64_MAX,old=read64((char*)reg+8);memcpy((char*)reg+8,&size,8);
 Adds=Maps=0;AddMemory(fdt);assert(!Adds && !Maps);memcpy((char*)reg+8,&old,8);
 int n=0;const void *prop=FdtGetProp(fdt,FdtPathOffset(fdt,"/memory"),"ddr_size",&n);assert(n==4);
 uint32_t bad=__builtin_bswap32(12);memcpy((void*)prop,&bad,4);
 Adds=Maps=0;AddMemory(fdt);assert(!Adds && !Maps);
 Plan=(void*)1;PlanCount=99;assert(!ShengGetPreparedMemoryMap(fdt,&Plan,&PlanCount));assert(!Plan && !PlanCount);
 puts("PASS - ABL qualification, reservations, owned GCD ranges, high bank, service failures, overlap, overflow and DDR");
 return 0;
}
