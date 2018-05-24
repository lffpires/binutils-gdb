/* Common target dependent code for FreeBSD on PPC systems.

   Copyright (C) 2018 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#ifndef ARCH_PPC_FBSD_COMMON_H
#define ARCH_PPC_FBSD_COMMON_H

#define PPC_FBSD_SIZEOF_GREGSET_32 148
#define PPC_FBSD_SIZEOF_GREGSET_64 296
#define PPC_FBSD_SIZEOF_FPREGSET 264

/* PT_GETVRREGS returns data as defined in machine/pcb.h:
   32 128-bit registers + 8 spare bytes + VRSAVE (4 bytes) + VSCR (4 bytes) */
#define PPC_FBSD_SIZEOF_VRREGSET (32*16 + 8 + 4 + 4)

typedef char gdb_vrregset_t[PPC_FBSD_SIZEOF_VRREGSET];

/* This is the layout of the POWER7 VSX registers and the way they overlap
   with the existing FPR and VMX registers.

                    VSR doubleword 0               VSR doubleword 1
           ----------------------------------------------------------------
   VSR[0]  |             FPR[0]            |                              |
           ----------------------------------------------------------------
   VSR[1]  |             FPR[1]            |                              |
           ----------------------------------------------------------------
           |              ...              |                              |
           |              ...              |                              |
           ----------------------------------------------------------------
   VSR[30] |             FPR[30]           |                              |
           ----------------------------------------------------------------
   VSR[31] |             FPR[31]           |                              |
           ----------------------------------------------------------------
   VSR[32] |                             VR[0]                            |
           ----------------------------------------------------------------
   VSR[33] |                             VR[1]                            |
           ----------------------------------------------------------------
           |                              ...                             |
           |                              ...                             |
           ----------------------------------------------------------------
   VSR[62] |                             VR[30]                           |
           ----------------------------------------------------------------
   VSR[63] |                             VR[31]                           |
          ----------------------------------------------------------------

   VSX has 64 128bit registers.  The first 32 registers overlap with
   the FP registers (doubleword 0) and hence extend them with additional
   64 bits (doubleword 1).  The other 32 regs overlap with the VMX
   registers.  */
#define PPC_FBSD_SIZEOF_VSXREGSET (32*8)

typedef char gdb_vsxregset_t[PPC_FBSD_SIZEOF_VSXREGSET];

/* Features used to determine the target description.  */
struct ppc_fbsd_features
{
  unsigned int wordsize;
  bool altivec;
  bool vsx;
};

/* Base value for ppc_fbsd_features variables.  */
const struct ppc_fbsd_features ppc_fbsd_no_features = {
  0,
  false,
  false
};

/* Return a target description that matches FEATURES.  */
const struct target_desc *
ppc_fbsd_match_description (struct ppc_fbsd_features features);

#endif /* ARCH_PPC_FBSD_COMMON_H */
