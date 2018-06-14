/* Native-dependent code for PowerPC's running FreeBSD, for GDB.

   Copyright (C) 2013-2018 Free Software Foundation, Inc.

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

#include "defs.h"
#include "gdbcore.h"
#include "inferior.h"
#include "regcache.h"
#include "regset.h"

#include <sys/types.h>
#include <sys/procfs.h>
#include <sys/ptrace.h>
#include <sys/signal.h>
#include <machine/frame.h>
#include <machine/pcb.h>
#include <machine/reg.h>

#include "fbsd-nat.h"
#include "gregset.h"
#include "ppc-tdep.h"
#include "ppc-fbsd-tdep.h"
#include "inf-ptrace.h"
#include "bsd-kvm.h"

#include "arch/ppc-fbsd-common.h"

/* These definitions should really come from machine/ptrace.h,
   but we provide them in case gdb is built on an machine whose
   FreeBSD version still doesn't have them.  */

#ifndef PT_FIRSTMACH
#define PT_FIRSTMACH 64
#endif

/* PTRACE requests for Altivec registers.  */
#ifndef PT_GETVRREGS
#define PT_GETVRREGS    (PT_FIRSTMACH + 0)
#define PT_SETVRREGS    (PT_FIRSTMACH + 1)
#endif

/* PTRACE requests for POWER7 VSX registers.  */
#ifndef PT_GETVSRREGS
#define PT_GETVSRREGS   (PT_FIRSTMACH + 2)
#define PT_SETVSRREGS   (PT_FIRSTMACH + 3)
#endif

typedef char gdb_vrregset_t[PPC_FBSD_SIZEOF_VRREGSET];
typedef char gdb_vsxregset_t[PPC_FBSD_SIZEOF_VSXREGSET];

struct ppc_fbsd_nat_target final : public fbsd_nat_target
{
  void fetch_registers (struct regcache *, int) override;
  void store_registers (struct regcache *, int) override;

  const struct target_desc *read_description () override;
};

static ppc_fbsd_nat_target the_ppc_fbsd_nat_target;

/* Fill GDB's register array with the general-purpose register values
   in *GREGSETP.  */

void
supply_gregset (struct regcache *regcache, const gdb_gregset_t *gregsetp)
{
  const struct regset *regset = ppc_fbsd_gregset (sizeof (long));

  ppc_supply_gregset (regset, regcache, -1, gregsetp, sizeof (*gregsetp));
}

/* Fill register REGNO (if a gpr) in *GREGSETP with the value in GDB's
   register array. If REGNO is -1 do it for all registers.  */

void
fill_gregset (const struct regcache *regcache,
	      gdb_gregset_t *gregsetp, int regno)
{
  const struct regset *regset = ppc_fbsd_gregset (sizeof (long));

  if (regno == -1)
    memset (gregsetp, 0, sizeof (*gregsetp));
  ppc_collect_gregset (regset, regcache, regno, gregsetp, sizeof (*gregsetp));
}

/* Fill GDB's register array with the floating-point register values
   in *FPREGSETP.  */

void
supply_fpregset (struct regcache *regcache, const gdb_fpregset_t * fpregsetp)
{
  const struct regset *regset = ppc_fbsd_fpregset ();

  ppc_supply_fpregset (regset, regcache, -1,
		       fpregsetp, sizeof (*fpregsetp));
}

/* Fill register REGNO in *FGREGSETP with the value in GDB's
   register array. If REGNO is -1 do it for all registers.  */

void
fill_fpregset (const struct regcache *regcache,
	       gdb_fpregset_t *fpregsetp, int regno)
{
  const struct regset *regset = ppc_fbsd_fpregset ();

  ppc_collect_fpregset (regset, regcache, regno,
			fpregsetp, sizeof (*fpregsetp));
}

/* Returns true if PT_GETFPREGS fetches this register.  */

static int
getfpregs_supplies (struct gdbarch *gdbarch, int regno)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  /* FIXME: jimb/2004-05-05: Some PPC variants don't have floating
	 point registers.  Traditionally, GDB's register set has still
	 listed the floating point registers for such machines, so this
	 code is harmless.  However, the new E500 port actually omits the
	 floating point registers entirely from the register set --- they
	 don't even have register numbers assigned to them.

	 It's not clear to me how best to update this code, so this assert
	 will alert the first person to encounter the NetBSD/E500
	 combination to the problem.  */

  gdb_assert (ppc_floating_point_unit_p (gdbarch));

  return ((regno >= tdep->ppc_fp0_regnum
	   && regno < tdep->ppc_fp0_regnum + ppc_num_fprs)
	  || regno == tdep->ppc_fpscr_regnum);
}

/* The kernel ptrace interface for AltiVec registers uses the
   registers set mechanism, as opposed to the interface for all the
   other registers, that stores/fetches each register individually.  */
static void
fetch_altivec_registers (struct regcache *regcache, int tid,
			 int regno)
{
  int ret;
  gdb_vrregset_t regs;
  const struct regset *vrregset = ppc_fbsd_vrregset ();

  ret = ptrace (PT_GETVRREGS, tid, (PTRACE_TYPE_ARG3) &regs, 0);
  if (ret < 0)
    perror_with_name (_("Unable to fetch AltiVec registers"));

  vrregset->supply_regset (vrregset, regcache, regno, &regs,
			   PPC_FBSD_SIZEOF_VRREGSET);
}

/* The kernel ptrace interface for POWER7 VSX registers uses the
   registers set mechanism, as opposed to the interface for all the
   other registers, that stores/fetches each register individually.  */
static void
fetch_vsx_registers (struct regcache *regcache, int tid, int regno)
{
  int ret;
  gdb_vsxregset_t regs;
  const struct regset *vsxregset = ppc_fbsd_vsxregset ();

  ret = ptrace (PT_GETVSRREGS, tid, (PTRACE_TYPE_ARG3) &regs, 0);
  if (ret < 0)
    perror_with_name (_("Unable to fetch VSX registers"));

  vsxregset->supply_regset (vsxregset, regcache, regno, &regs,
			    PPC_FBSD_SIZEOF_VSXREGSET);
}

static void
store_altivec_registers (const struct regcache *regcache, int tid,
			 int regno)
{
  int ret;
  gdb_vrregset_t regs;
  const struct regset *vrregset = ppc_fbsd_vrregset ();

  ret = ptrace (PT_GETVRREGS, tid, (PTRACE_TYPE_ARG3) &regs, 0);
  if (ret < 0)
    perror_with_name (_("Unable to fetch AltiVec registers"));

  vrregset->collect_regset (vrregset, regcache, regno, &regs,
			    PPC_FBSD_SIZEOF_VRREGSET);

  ret = ptrace (PT_SETVRREGS, tid, (PTRACE_TYPE_ARG3) &regs, 0);
  if (ret < 0)
    perror_with_name (_("Unable to store AltiVec registers"));
}

static void
store_vsx_registers (const struct regcache *regcache, int tid, int regno)
{
  int ret;
  gdb_vsxregset_t regs;
  const struct regset *vsxregset = ppc_fbsd_vsxregset ();

  ret = ptrace (PT_GETVSRREGS, tid, (PTRACE_TYPE_ARG3) &regs, 0);
  if (ret < 0)
    perror_with_name (_("Unable to fetch VSX registers"));

  vsxregset->collect_regset (vsxregset, regcache, regno, &regs,
			     PPC_FBSD_SIZEOF_VSXREGSET);

  ret = ptrace (PT_SETVSRREGS, tid, (PTRACE_TYPE_ARG3) &regs, 0);
  if (ret < 0)
    perror_with_name (_("Unable to store VSX registers"));
}

static int
ppc_fbsd_target_wordsize (int tid)
{
  int wordsize = 4;

#ifdef __powerpc64__
  /* Check for 64-bit inferior process.  This is the case when the host is
     64-bit, and PT_GETREGS returns less data than the length of gdb_gregset_t.  */

  gdb_gregset_t regs0;
  gdb_gregset_t regs1;

  /* Initialize regs0 with 00's and regs1 with ff's. If, after ptrace fills
     them, they have the same contents, it means ptrace returned data for a
     64-bit inferior.  */
  memset (&regs0, 0, sizeof (regs0));
  memset (&regs1, 0xff, sizeof (regs1));

  if (ptrace (PT_GETREGS, tid, (PTRACE_TYPE_ARG3) &regs0, 0) >= 0
      && ptrace (PT_GETREGS, tid, (PTRACE_TYPE_ARG3) &regs1, 0) >= 0)
    if (memcmp (&regs0, &regs1, sizeof (regs0)) == 0)
      wordsize = 8;
#endif

  return wordsize;
}

/* Fetch register REGNO from the child process. If REGNO is -1, do it
   for all registers.  */

void
ppc_fbsd_nat_target::fetch_registers (struct regcache *regcache, int regno)
{
  gdb_gregset_t regs;
  pid_t pid = ptid_get_lwp (regcache->ptid ());
  struct gdbarch *gdbarch = regcache->arch ();
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  if (ptrace (PT_GETREGS, pid, (PTRACE_TYPE_ARG3) &regs, 0) == -1)
    perror_with_name (_("Couldn't get registers"));

  supply_gregset (regcache, &regs);

  if (regno == -1 || getfpregs_supplies (gdbarch, regno))
    {
      const struct regset *fpregset = ppc_fbsd_fpregset ();
      gdb_fpregset_t fpregs;

      if (ptrace (PT_GETFPREGS, pid, (PTRACE_TYPE_ARG3) &fpregs, 0) == -1)
	perror_with_name (_("Couldn't get FP registers"));

      ppc_supply_fpregset (fpregset, regcache, regno, &fpregs, sizeof (fpregs));
    }

  if (tdep->ppc_vr0_regnum != -1 && tdep->ppc_vrsave_regnum != -1)
    if (regno == -1 || altivec_register_p (gdbarch, regno))
      fetch_altivec_registers (regcache, pid, regno);

  if (tdep->ppc_vsr0_upper_regnum != -1)
    if (regno == -1 || vsx_register_p (gdbarch, regno))
      fetch_vsx_registers (regcache, pid, regno);
}

/* Store register REGNO back into the child process. If REGNO is -1,
   do this for all registers.  */

void
ppc_fbsd_nat_target::store_registers (struct regcache *regcache, int regno)
{
  gdb_gregset_t regs;
  pid_t pid = ptid_get_lwp (regcache->ptid ());
  struct gdbarch *gdbarch = regcache->arch ();
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  if (ptrace (PT_GETREGS, pid, (PTRACE_TYPE_ARG3) &regs, 0) == -1)
    perror_with_name (_("Couldn't get registers"));

  fill_gregset (regcache, &regs, regno);

  if (ptrace (PT_SETREGS, pid, (PTRACE_TYPE_ARG3) &regs, 0) == -1)
    perror_with_name (_("Couldn't write registers"));

  if (regno == -1 || getfpregs_supplies (gdbarch, regno))
    {
      gdb_fpregset_t fpregs;

      if (ptrace (PT_GETFPREGS, pid, (PTRACE_TYPE_ARG3) &fpregs, 0) == -1)
	perror_with_name (_("Couldn't get FP registers"));

      fill_fpregset (regcache, &fpregs, regno);

      if (ptrace (PT_SETFPREGS, pid, (PTRACE_TYPE_ARG3) &fpregs, 0) == -1)
	perror_with_name (_("Couldn't set FP registers"));
    }

  if (tdep->ppc_vr0_regnum != -1 && tdep->ppc_vrsave_regnum != -1)
    if (regno == -1 || altivec_register_p (gdbarch, regno))
      store_altivec_registers (regcache, pid, regno);

  if (tdep->ppc_vsr0_upper_regnum != -1)
    if (regno == -1 || vsx_register_p (gdbarch, regno))
      store_vsx_registers (regcache, pid, regno);
}

const struct target_desc *
ppc_fbsd_nat_target::read_description ()
{
  int tid = ptid_get_lwp (inferior_ptid);

  struct ppc_fbsd_features features = ppc_fbsd_no_features;

  features.wordsize = ppc_fbsd_target_wordsize (tid);

  gdb_vsxregset_t vsxregset;

  if (ptrace (PT_GETVSRREGS, tid, (PTRACE_TYPE_ARG3) &vsxregset, 0) >= 0)
      features.vsx = true;

  /* EINVAL means that the PT_GETVSRREGS request isn't supported.
     Anything else needs to be reported.  */
  else if (errno != EINVAL)
    perror_with_name (_("Unable to fetch VSX registers"));

  gdb_vrregset_t vrregset;

  if (ptrace (PT_GETVRREGS, tid, (PTRACE_TYPE_ARG3) &vrregset, 0) >= 0)
    features.altivec = true;

  /* EINVAL means that the PT_GETVRREGS request isn't supported.
     Anything else needs to be reported.  */
  else if (errno != EINVAL)
    perror_with_name (_("Unable to fetch AltiVec registers"));

  return ppc_fbsd_match_description (features);
}

/* Architecture specific function that reconstructs the
   register state from PCB (Process Control Block) and supplies it
   to REGCACHE.  */

static int
ppcfbsd_supply_pcb (struct regcache *regcache, struct pcb *pcb)
{
  struct gdbarch *gdbarch = regcache->arch ();
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  int i, regnum;

  /* The stack pointer shouldn't be zero.  */
  if (pcb->pcb_sp == 0)
    return 0;

  regcache->raw_supply (gdbarch_sp_regnum (gdbarch), &pcb->pcb_sp);
  regcache->raw_supply (tdep->ppc_cr_regnum, &pcb->pcb_cr);
  regcache->raw_supply (tdep->ppc_lr_regnum, &pcb->pcb_lr);
  for (i = 0, regnum = tdep->ppc_gp0_regnum + 14; i < 20; i++, regnum++)
    regcache->raw_supply (regnum, &pcb->pcb_context[i]);

  return 1;
}

void
_initialize_ppcfbsd_nat (void)
{
  add_inf_child_target (&the_ppc_fbsd_nat_target);

  /* Support debugging kernel virtual memory images.  */
  bsd_kvm_add_target (ppcfbsd_supply_pcb);
}
