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

#include "common-defs.h"
#include "arch/ppc-fbsd-common.h"

extern struct target_desc *tdesc_powerpc_32;
extern struct target_desc *tdesc_powerpc_altivec32;
extern struct target_desc *tdesc_powerpc_vsx32;
extern struct target_desc *tdesc_powerpc_64;
extern struct target_desc *tdesc_powerpc_altivec64;
extern struct target_desc *tdesc_powerpc_vsx64;

const struct target_desc *
ppc_fbsd_match_description (struct ppc_fbsd_features features)
{
  struct target_desc *tdesc = NULL;

  if (features.wordsize == 8)
    {
      if (features.vsx)
	tdesc = tdesc_powerpc_vsx64;
      else if (features.altivec)
	tdesc = tdesc_powerpc_altivec64;
      else
	tdesc = tdesc_powerpc_64;
    }
  else
    {
      gdb_assert (features.wordsize == 4);

      if (features.vsx)
	tdesc = tdesc_powerpc_vsx32;
      else if (features.altivec)
	tdesc = tdesc_powerpc_altivec32;
      else
	tdesc = tdesc_powerpc_32;
    }

  gdb_assert (tdesc != NULL);

  return tdesc;
}
