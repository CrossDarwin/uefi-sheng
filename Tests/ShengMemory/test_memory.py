#!/usr/bin/env python3
"""Exercise the production ABL policy and DXE publication with host service substitutes."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
DTS = '''/dts-v1/;
/ { #address-cells = <2>; #size-cells = <2>;
 memory { device_type = "memory"; ddr_size = <8>;
 reg = <0 0x80000000 0 0x58000000>, <0 0xe7cb0000 0 0x18350000>,
       <8 0x80000000 0 0x3b300000>, <8 0xbb500000 0 0x1e00000>,
       <8 0xc0000000 1 0x40000000>; };
 chosen { linux,initrd-start = <8 0xd0000000>; linux,initrd-end = <8 0xd0200000>; };
 reserved-memory { #address-cells = <2>; #size-cells = <2>; ranges;
   hyp@80000000 { reg = <0 0x80000000 0 0x11d0000>; };
   mpss_region@89800000 { reg = <0 0x89800000 0 0x10800000>; status = "MPSS"; };
 }; };
'''


def main():
    with tempfile.TemporaryDirectory(prefix='sheng-memory-') as name:
        scratch = Path(name)
        for header in ('Uefi.h', 'Library/ArmMmuLib.h', 'Library/BaseLib.h',
                       'Library/BaseMemoryLib.h', 'Library/DebugLib.h', 'Library/FdtLib.h',
                       'Library/HobLib.h', 'Library/MemoryAllocationLib.h',
                       'Library/MemoryMapLib.h', 'Library/DxeServicesTableLib.h',
                       'Library/UefiBootServicesTableLib.h'):
            path = scratch / header
            path.parent.mkdir(parents=True, exist_ok=True)
            path.touch()
        binary = scratch / 'memory'
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-D_GNU_SOURCE',
                        '-Wall', '-Wextra', '-Wno-unused-parameter', '-I', str(scratch),
                        '-I', str(HERE), str(HERE / 'Policy.c'), '-Wl,-l:libfdt.so.1',
                        '-o', str(binary)], check=True)
        for state in ('okay', 'disabled'):
            source = scratch / 'memory.dts'
            target = scratch / 'memory.dtb'
            source.write_text(DTS.replace('MPSS', state))
            subprocess.run(['dtc', '-q', '-I', 'dts', '-O', 'dtb', '-o', str(target), str(source)], check=True)
            subprocess.run([str(binary), str(target)], check=True)


if __name__ == '__main__':
    main()
