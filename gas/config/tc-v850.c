/* tc-v850.c -- Assembler code for the NEC V850

   Copyright (C) 1996 Free Software Foundation.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to
   the Free Software Foundation, 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include <stdio.h>
#include <ctype.h>
#include "as.h"
#include "subsegs.h"     
#include "opcode/v850.h"

/* Structure to hold information about predefined registers.  */
struct reg_name
{
  const char *name;
  int value;
};

/* Generic assembler global variables which must be defined by all targets. */

/* Characters which always start a comment. */
const char comment_chars[] = "#";

/* Characters which start a comment at the beginning of a line.  */
const char line_comment_chars[] = ";#";

/* Characters which may be used to separate multiple commands on a 
   single line.  */
const char line_separator_chars[] = ";";

/* Characters which are used to indicate an exponent in a floating 
   point number.  */
const char EXP_CHARS[] = "eE";

/* Characters which mean that a number is a floating point constant, 
   as in 0d1.0.  */
const char FLT_CHARS[] = "dD";

/* local functions */
static unsigned long v850_insert_operand
  PARAMS ((unsigned long insn, const struct v850_operand *operand,
	   offsetT val, char *file, unsigned int line));
static int reg_name_search PARAMS ((char *name, const struct reg_name *, int));
static boolean register_name PARAMS ((expressionS *expressionP));
static boolean system_register_name PARAMS ((expressionS *expressionP));
static int postfix PARAMS ((char *p));
static bfd_reloc_code_real_type get_reloc PARAMS ((struct v850_operand *op));
static unsigned long build_insn PARAMS ((struct v850_opcode *opcode, expressionS *opers));


/* fixups */
#define MAX_INSN_FIXUPS (5)
struct v850_fixup
{
  expressionS exp;
  int opindex;
  bfd_reloc_code_real_type reloc;
};
struct v850_fixup fixups[MAX_INSN_FIXUPS];
static int fc;

const char *md_shortopts = "";
struct option md_longopts[] = {
  {NULL, no_argument, NULL, 0}
};
size_t md_longopts_size = sizeof(md_longopts); 

/* The target specific pseudo-ops which we support.  */
const pseudo_typeS md_pseudo_table[] =
{
  /*
  { "byte",     ppc_byte,       0 },
  { "long",     ppc_elf_cons,   4 },
  { "word",     ppc_elf_cons,   2 },
  { "short",    ppc_elf_cons,   2 },
  { "rdata",    ppc_elf_rdata,  0 },
  { "rodata",   ppc_elf_rdata,  0 },
  { "lcomm",    ppc_elf_lcomm,  0 },
  */
  { NULL,       NULL,           0 }
};

/* Opcode hash table.  */
static struct hash_control *v850_hash;

/* This table is sorted. Suitable for searching by a binary search. */
static const struct reg_name pre_defined_registers[] =
{
  { "ep", 30 },			/* ep - element ptr */
  { "gp", 4 },			/* gp - global ptr */
  { "lp", 31 },			/* lp - link ptr */
  { "r0", 0 },
  { "r1", 1 },
  { "r10", 10 },
  { "r11", 11 },
  { "r12", 12 },
  { "r13", 13 },
  { "r14", 14 },
  { "r15", 15 },
  { "r16", 16 },
  { "r17", 17 },
  { "r18", 18 },
  { "r19", 19 },
  { "r2", 2 },
  { "r20", 20 },
  { "r21", 21 },
  { "r22", 22 },
  { "r23", 23 },
  { "r24", 24 },
  { "r25", 25 },
  { "r26", 26 },
  { "r27", 27 },
  { "r28", 28 },
  { "r29", 29 },
  { "r3", 3 },
  { "r30", 30 },
  { "r31", 31 },
  { "r4", 4 },
  { "r5", 5 },
  { "r6", 6 },
  { "r7", 7 },
  { "r8", 8 },
  { "r9", 9 },
  { "sp", 3 },			/* sp - stack ptr */
  { "tp", 5 },			/* tp - text ptr */
  { "zero", 0 },
};
#define REG_NAME_CNT	(sizeof(pre_defined_registers) / sizeof(struct reg_name))


static const struct reg_name system_registers[] = 
{
  { "eipc", 0 },
  { "eipsw", 1 },
  { "fepc", 2 },
  { "fepsw", 3 },
  { "ecr", 4 },
  { "psw", 5 },
};
#define SYSREG_NAME_CNT	(sizeof(system_registers) / sizeof(struct reg_name))

static const struct reg_name cc_names[] =
{
  { "c", 0x1 },
  { "ge", 0xe },
  { "gt", 0xf },
  { "h", 0xb },
  { "l", 0x1 },
  { "le", 0x7 },
  { "lt", 0x6 },
  { "n", 0x4 },
  { "nc", 0x9 },
  { "nh", 0x3 },
  { "nl", 0x9 },
  { "ns", 0xc },
  { "nv", 0x8 },
  { "nz", 0xa },
  { "p",  0xc },
  { "s", 0x4 },
  { "sa", 0xd },
  { "t", 0x5 },
  { "v", 0x0 },
  { "z", 0x2 },
};

/* reg_name_search does a binary search of the given register table
   to see if "name" is a valid regiter name.  Returns the register
   number from the array on success, or -1 on failure. */

static int
reg_name_search (name, table, high)
     char *name;
     const struct reg_name *table;
     int high;
{
  int middle, low;
  int cmp;

  low = 0;
  do
    {
      middle = (low + high) / 2;
      cmp = strcasecmp (name, table[middle].name);
      if (cmp < 0)
	high = middle - 1;
      else if (cmp > 0)
	low = middle + 1;
      else 
	  return table[middle].value;
    }
  while (low <= high);
  return -1;
}


/* Summary of register_name().
 *
 * in: Input_line_pointer points to 1st char of operand.
 *
 * out: A expressionS.
 *	The operand may have been a register: in this case, X_op == O_register,
 *	X_add_number is set to the register number, and truth is returned.
 *	Input_line_pointer->(next non-blank) char after operand, or is in
 *	its original state.
 */
static boolean
register_name (expressionP)
     expressionS *expressionP;
{
  int reg_number;
  char *name;
  char *start;
  char c;

  /* Find the spelling of the operand */
  start = name = input_line_pointer;

  c = get_symbol_end ();
  reg_number = reg_name_search (name, pre_defined_registers, REG_NAME_CNT - 1);

  /* look to see if it's in the register table */
  if (reg_number >= 0) 
    {
      expressionP->X_op = O_register;
      expressionP->X_add_number = reg_number;

      /* make the rest nice */
      expressionP->X_add_symbol = NULL;
      expressionP->X_op_symbol = NULL;
      *input_line_pointer = c;	/* put back the delimiting char */
      return true;
    }
  else
    {
      /* reset the line as if we had not done anything */
      *input_line_pointer = c;   /* put back the delimiting char */
      input_line_pointer = start; /* reset input_line pointer */
      return false;
    }
}

/* Summary of system_register_name().
 *
 * in: Input_line_pointer points to 1st char of operand.
 *
 * out: A expressionS.
 *	The operand may have been a register: in this case, X_op == O_register,
 *	X_add_number is set to the register number, and truth is returned.
 *	Input_line_pointer->(next non-blank) char after operand, or is in
 *	its original state.
 */
static boolean
system_register_name (expressionP)
     expressionS *expressionP;
{
  int reg_number;
  char *name;
  char *start;
  char c;

  /* Find the spelling of the operand */
  start = name = input_line_pointer;

  c = get_symbol_end ();
  reg_number = reg_name_search (name, system_registers, SYSREG_NAME_CNT - 1);

  /* look to see if it's in the register table */
  if (reg_number >= 0) 
    {
      expressionP->X_op = O_register;
      expressionP->X_add_number = reg_number;

      /* make the rest nice */
      expressionP->X_add_symbol = NULL;
      expressionP->X_op_symbol = NULL;
      *input_line_pointer = c;	/* put back the delimiting char */
      return true;
    }
  else
    {
      /* reset the line as if we had not done anything */
      *input_line_pointer = c;   /* put back the delimiting char */
      input_line_pointer = start; /* reset input_line pointer */
      return false;
    }
}

void
md_show_usage (stream)
  FILE *stream;
{
  fprintf(stream, "V850 options:\n\
none yet\n");
} 

int
md_parse_option (c, arg)
     int c;
     char *arg;
{
  return 0;
}

symbolS *
md_undefined_symbol (name)
  char *name;
{
  return 0;
}

char *
md_atof (type, litp, sizep)
  int type;
  char *litp;
  int *sizep;
{
  int prec;
  LITTLENUM_TYPE words[4];
  char *t;
  int i;

  switch (type)
    {
    case 'f':
      prec = 2;
      break;

    case 'd':
      prec = 4;
      break;

    default:
      *sizep = 0;
      return "bad call to md_atof";
    }
  
  t = atof_ieee (input_line_pointer, type, words);
  if (t)
    input_line_pointer = t;

  *sizep = prec * 2;

  for (i = prec - 1; i >= 0; i--)
    {
      md_number_to_chars (litp, (valueT) words[i], 2);
      litp += 2;
    }

  return NULL;
}


void
md_convert_frag (abfd, sec, fragP)
  bfd *abfd;
  asection *sec;
  fragS *fragP;
{
  /* printf ("call to md_convert_frag \n"); */
  abort ();
}

valueT
md_section_align (seg, addr)
     asection *seg;
     valueT addr;
{
  int align = bfd_get_section_alignment (stdoutput, seg);
  return ((addr + (1 << align) - 1) & (-1 << align));
}

void
md_begin ()
{
  char *prev_name = "";
  register const struct v850_opcode *op;
  const struct v850_opcode *op_end;

  v850_hash = hash_new();

  /* Insert unique names into hash table.  The V850 instruction set
     has many identical opcode names that have different opcodes based
     on the operands.  This hash table then provides a quick index to
     the first opcode with a particular name in the opcode table.  */

  op     = v850_opcodes;
  op_end = v850_opcodes + v850_num_opcodes;

  for (; op < op_end; op++)
    {
      if (strcmp (prev_name, op->name)) 
	{
	  prev_name = (char *) op->name;
	  hash_insert (v850_hash, op->name, (char *) op);
	}
    }
}


static bfd_reloc_code_real_type 
get_reloc (op) 
     struct v850_operand *op;
{
  abort ();
}


void
md_assemble (str) 
     char *str;
{
  char *s;
  struct v850_opcode *opcode;
  struct v850_opcode *next_opcode;
  const unsigned char *opindex_ptr;
  int next_opindex;
  unsigned long insn;
  char *f;
  int i;
  int numops;
  int match;

  int numopts;
  expressionS myops[5];

  /* Get the opcode.  */
  for (s = str; *s != '\0' && ! isspace (*s); s++)
    ;
  if (*s != '\0')
    *s++ = '\0';

  /* find the first opcode with the proper name */
  opcode = (struct v850_opcode *)hash_find (v850_hash, str);
  if (opcode == NULL)
    {
      as_bad ("Unrecognized opcode: `%s'", str);
      return;
    }

  str = s;
  while (isspace (*str))
    ++str;

  input_line_pointer = str;

  for(;;)
    {
      const char *errmsg = NULL;

      fc = 0;
      match = 0;
      next_opindex = 0;
      insn = opcode->opcode;
      for (opindex_ptr = opcode->operands; *opindex_ptr != 0; opindex_ptr++)
	{
	  const struct v850_operand *operand;
	  char *hold;
	  expressionS ex;
	  char endc;

	  if (next_opindex == 0)
	    {
	      operand = &v850_operands[*opindex_ptr];
	    }
	  else
	    {
	      operand = &v850_operands[next_opindex];
	      next_opindex = 0;
	    }

	  errmsg = NULL;

	  while (*str == ' ' || *str == ',' || *str == '[' || *str == ']')
	    ++str;

	  /* Gather the operand. */
	  hold = input_line_pointer;
	  input_line_pointer = str;

	  if ((operand->flags & V850_OPERAND_REG) != 0) 
	    {
	      if (!register_name(&ex))
		{
		  errmsg = "invalid register name";
		  goto error;
		}
	    }
	  else if ((operand->flags & V850_OPERAND_SRG) != 0) 
	    {
	      if (!system_register_name(&ex))
		{
		  errmsg = "invalid system register name";
		  goto error;
		}
	    }
	  else if (strncmp(input_line_pointer, "lo(", 3) == 0) 
	    {
	      input_line_pointer += 3;
	      expression(&ex);

	      if (*input_line_pointer++ != ')')
		{
		  errmsg = "syntax error: expected `)'";
		  goto error;
		}
	      
	      if (ex.X_op == O_constant) 
		ex.X_add_number &= 0xffff;
	      else
		{
		  if (fc > MAX_INSN_FIXUPS)
		    as_fatal ("too many fixups");
		  
		  fixups[fc].exp = ex;
		  fixups[fc].opindex = *opindex_ptr;
		  fixups[fc].reloc = BFD_RELOC_LO16;
		  fc++;
		}
	    }
	  else if (strncmp(input_line_pointer, "hi(", 3) == 0) 
	    {
	      input_line_pointer += 3;
	      expression(&ex);

	      if (*input_line_pointer++ != ')')
		{
		  errmsg = "syntax error: expected `)'";
		  goto error;
		}
	      
	      if (ex.X_op == O_constant)
		ex.X_add_number = (ex.X_add_number >> 16) & 0xffff;
	      else 
		{
		  if (fc > MAX_INSN_FIXUPS)
		    as_fatal ("too many fixups");
		  
		  fixups[fc].exp = ex;
		  fixups[fc].opindex = *opindex_ptr;
		  fixups[fc].reloc = BFD_RELOC_HI16;
		  fc++;
		}
	    }
	  else if (register_name (&ex)
		   && (operand->flags & V850_OPERAND_REG) == 0)
	    {
	      errmsg = "syntax error: register not expected";
	      goto error;
	    }
	  else if (system_register_name (&ex)
		   && (operand->flags & V850_OPERAND_SRG) == 0)
	    {
	      errmsg = "syntax error: system register not expected";
	      goto error;
	    }
	  else
	    {
		expression(&ex);
	    }

	  str = input_line_pointer;
	  input_line_pointer = hold;

	  switch (ex.X_op) 
	    {
	    case O_illegal:
	      errmsg = "illegal operand";
	      goto error;
	    case O_absent:
	      errmsg = "missing operand";
	      goto error;
	    case O_register:
	      if ((operand->flags & (V850_OPERAND_REG | V850_OPERAND_SRG)) == 0)
		{
		  errmsg = "invalid operand";
		  goto error;
		}
		
	      insn = v850_insert_operand (insn, operand, ex.X_add_number,
					  (char *) NULL, 0);
	      break;

	    case O_constant:
	      insn = v850_insert_operand (insn, operand, ex.X_add_number,
					  (char *) NULL, 0);
	      break;

	    default:
	      /* We need to generate a fixup for this expression.  */
	      if (fc >= MAX_INSN_FIXUPS)
		as_fatal ("too many fixups");
	      fixups[fc].exp = ex;
	      fixups[fc].opindex = *opindex_ptr;
	      fixups[fc].reloc = BFD_RELOC_UNUSED;
	      ++fc;
	      break;
	    }

	  while (*str == ' ' || *str == ',' || *str == '[' || *str == ']')
	    ++str;
	}
      match = 1;

    error:
      if (match == 0)
        {
	  next_opcode = opcode + 1;
	  if (next_opcode->opcode != 0 && !strcmp(next_opcode->name, opcode->name))
	    {
	      opcode = next_opcode;
	      continue;
	    }
	  
	  as_bad ("%s", errmsg);
	  return;
        }
      break;
    }
      
  while (isspace (*str))
    ++str;

  if (*str != '\0')
    as_bad ("junk at end of line: `%s'", str);

  input_line_pointer = str;

  f = frag_more (opcode->size);
  md_number_to_chars (f, insn, opcode->size);
}


/* if while processing a fixup, a reloc really needs to be created */
/* then it is done here */
                 
arelent *
tc_gen_reloc (seg, fixp)
     asection *seg;
     fixS *fixp;
{
  arelent *reloc;
  reloc = (arelent *) bfd_alloc_by_size_t (stdoutput, sizeof (arelent));
  reloc->sym_ptr_ptr = &fixp->fx_addsy->bsym;
  reloc->address = fixp->fx_frag->fr_address + fixp->fx_where;
  reloc->howto = bfd_reloc_type_lookup (stdoutput, fixp->fx_r_type);
  if (reloc->howto == (reloc_howto_type *) NULL)
    {
      as_bad_where (fixp->fx_file, fixp->fx_line,
                    "reloc %d not supported by object file format", (int)fixp->fx_r_type);
      return NULL;
    }
  reloc->addend = fixp->fx_addnumber;
  /*  printf("tc_gen_reloc: addr=%x  addend=%x\n", reloc->address, reloc->addend); */
  return reloc;
}

int
md_estimate_size_before_relax (fragp, seg)
     fragS *fragp;
     asection *seg;
{
  abort ();
  return 0;
} 

long
md_pcrel_from_section (fixp, sec)
     fixS *fixp;
     segT sec;
{
    return 0;
    /*  return fixp->fx_frag->fr_address + fixp->fx_where; */
}

int
md_apply_fix3 (fixp, valuep, seg)
     fixS *fixp;
     valueT *valuep;
     segT seg;
{
  abort();
#if 0
  valueT value;
  char *where;
  unsigned long insn;
  int op_type;

  if (fixp->fx_addsy == (symbolS *) NULL)
    {
      value = *valuep;
      fixp->fx_done = 1;
    }
  else if (fixp->fx_pcrel)
    value = *valuep;
  else
    {
      value = fixp->fx_offset;
      if (fixp->fx_subsy != (symbolS *) NULL)
	{
	  if (S_GET_SEGMENT (fixp->fx_subsy) == absolute_section)
	    value -= S_GET_VALUE (fixp->fx_subsy);
	  else
	    {
	      /* We don't actually support subtracting a symbol.  */
	      as_bad_where (fixp->fx_file, fixp->fx_line,
			    "expression too complex");
	    }
	}
    }
  
  /*   printf("md_apply_fix: value=0x%x  type=%d\n",  value, fixp->fx_r_type); */

  op_type = fixp->fx_r_type;
  fixp->fx_r_type = get_reloc((struct v850_operand *)&v850_operands[op_type]); 

  /*  printf("reloc=%d\n",fixp->fx_r_type); */

  /* Fetch the instruction, insert the fully resolved operand
     value, and stuff the instruction back again.  */
  where = fixp->fx_frag->fr_literal + fixp->fx_where;
  insn = bfd_getb32 ((unsigned char *) where);
  /* printf("   insn=%x  value=%x\n",insn,value); */

  insn = v850_insert_operand (insn, op_type, (offsetT) value);
  
  /* printf("   new insn=%x\n",insn); */
  
  bfd_putb32 ((bfd_vma) insn, (unsigned char *) where);
  
  if (fixp->fx_done)
    return 1;

  fixp->fx_addnumber = value;
  return 1;
#endif
}


/* Insert an operand value into an instruction.  */

static unsigned long
v850_insert_operand (insn, operand, val, file, line)
     unsigned long insn;
     const struct v850_operand *operand;
     offsetT val;
     char *file;
     unsigned int line;
{
  if (operand->bits != 32)
    {
      long min, max;
      offsetT test;

      if ((operand->flags & V850_OPERAND_SIGNED) != 0)
        {
#if 0
          if ((operand->flags & PPC_OPERAND_SIGNOPT) != 0
              && ppc_size == PPC_OPCODE_32)
            max = (1 << operand->bits) - 1;
          else
#endif
            max = (1 << (operand->bits - 1)) - 1;
          min = - (1 << (operand->bits - 1));
        }
      else
        {
          max = (1 << operand->bits) - 1;
          min = 0;
        }

#if 0
      if ((operand->flags & PPC_OPERAND_NEGATIVE) != 0)
        test = - val;
      else
#endif
        test = val;


      if (test < (offsetT) min || test > (offsetT) max)
        {
          const char *err =
            "operand out of range (%s not between %ld and %ld)";
          char buf[100];

          sprint_value (buf, test);
          if (file == (char *) NULL)
            as_warn (err, buf, min, max);
          else
            as_warn_where (file, line, err, buf, min, max);
        }
    }

  insn |= (((long) val & ((1 << operand->bits) - 1)) << operand->shift);
  return insn;
}
