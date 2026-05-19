/*
 * tracs_assembler.c
 *
 * TRACS (TRAnsition semester Computer System) Assembler
 * CpE 3202 - Computer Organization & Architecture
 * University of San Carlos
 *
 * Implements a two-pass assembler for the TRACS ISA as specified in:
 *   - TRACS Assembler Briefing v1.0 (02-TRACS-Assembler-Briefing-v1.0-2526.pdf)
 *   - Introduction to TRACS (03-CpE3202-3.3-Introduction-to-TRACS.pdf)
 *   - TRACS Assembler Flowchart (TRACS-assembler-flowchart.pdf)
 *
 * Build:
 *   gcc -std=c99 -Wall -Wextra -pedantic -o tracs_assembler tracs_assembler.c
 *
 * Usage:
 *   ./tracs_assembler <input.asm> [output.txt]
 *   If output filename is omitted, the output file takes the base name of
 *   the input file with .txt extension.
 *
 * ============================================================
 * TRACS ISA Summary (all 24 instructions)
 * ============================================================
 *
 * Instruction format: 16-bit word, big-endian in memory.
 *   Bits 15..11 : 5-bit instruction code (opcode)
 *   Bits 10..0  : 11-bit operand (address or data; 0 if unused)
 *
 * For data-immediate operands (WB, WIB):
 *   The 8-bit literal is stored in bits 7..0; bits 10..8 are 0.
 *
 * For address operands (WM, RM, WIO, RIO, BR, BRE, BRNE, BRGT, BRLT):
 *   The 11-bit address is stored in bits 10..0.
 *
 * For implied operands (ADD, SUB, MUL, AND, OR, NOT, XOR, SHL, SHR,
 *                       WACC, RACC, SWAP, EOP):
 *   Bits 10..0 are set to 0.
 *
 * Machine code is stored big-endian: high byte at ADDR, low byte at ADDR+1.
 * Each instruction occupies two consecutive byte addresses.
 *
 * Opcode table (from C source Team-3-LE6_CPU-Memory-IO.c):
 *   WM   = 0x01 (00001b)  address operand  - Write MBR to data memory
 *   RM   = 0x02 (00010b)  address operand  - Read data memory to MBR
 *   BR   = 0x03 (00011b)  address operand  - Unconditional branch
 *   RIO  = 0x04 (00100b)  address operand  - Read I/O memory to IOBR
 *   WIO  = 0x05 (00101b)  address operand  - Write IOBR to I/O memory
 *   WB   = 0x06 (00110b)  data operand     - Write literal to MBR
 *   WIB  = 0x07 (00111b)  data operand     - Write literal to IOBR
 *   WACC = 0x09 (01001b)  implied          - Write BUS (MBR) to ACC
 *   RACC = 0x0B (01011b)  implied          - Read ACC to BUS (MBR)
 *   SWAP = 0x0E (01110b)  implied          - Swap MBR and IOBR
 *   BRLT = 0x11 (10001b)  address operand  - Branch if ACC < BUS (SF=1)
 *   BRGT = 0x12 (10010b)  address operand  - Branch if ACC > BUS (SF=0,ZF=0)
 *   BRNE = 0x13 (10011b)  address operand  - Branch if ACC != BUS (ZF=0)
 *   BRE  = 0x14 (10100b)  address operand  - Branch if ACC == BUS (ZF=1)
 *   SHR  = 0x15 (10101b)  implied          - Shift ACC right 1 bit
 *   SHL  = 0x16 (10110b)  implied          - Shift ACC left 1 bit
 *   XOR  = 0x17 (10111b)  implied          - ACC ^= BUS
 *   NOT  = 0x18 (11000b)  implied          - ACC = ~ACC
 *   OR   = 0x19 (11001b)  implied          - ACC |= BUS
 *   AND  = 0x1A (11010b)  implied          - ACC &= BUS
 *   MUL  = 0x1B (11011b)  implied          - ACC *= BUS
 *   SUB  = 0x1D (11101b)  implied          - ACC -= BUS
 *   ADD  = 0x1E (11110b)  implied          - ACC += BUS
 *   EOP  = 0x1F (11111b)  implied          - End of Program
 *
 * Address ranges (from spec):
 *   Program memory : 0x000 – 0x3FF (word instructions at even byte addresses)
 *   Data memory    : 0x400 – 0x7FF (operand for WM/RM)
 *   I/O memory     : 0x000 – 0x01F (operand for WIO/RIO)
 *
 * ============================================================
 * Flowchart Mapping
 * ============================================================
 *
 * Top-level   -> main(): open file, call pass1, pass2, error check, report
 * Pass 1      -> pass1(): scan for ORG directive and labels, build symbol table
 * Pass 2      -> pass2(): re-scan, look up labels, encode, write .txt output
 * Error check -> validate_and_report(): four mandatory error checks
 *
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

/* ------------------------------------------------------------------ */
/*  Constants                                                          */
/* ------------------------------------------------------------------ */

#define MAX_LINE_LEN      256
#define MAX_LABEL_LEN      64
#define MAX_MNEMONIC_LEN   16
#define MAX_OPERAND_LEN    32
#define MAX_LABELS        256
#define MAX_INSTRUCTIONS 2048

/* Memory address limits */
#define PROG_MEM_MIN  0x000u
#define PROG_MEM_MAX  0x3FFu
#define DATA_MEM_MIN  0x400u
#define DATA_MEM_MAX  0x7FFu
#define IO_MEM_MIN    0x000u
#define IO_MEM_MAX    0x01Fu

/* ------------------------------------------------------------------ */
/*  Operand type classification                                        */
/* ------------------------------------------------------------------ */
typedef enum {
    OP_NONE,      /* no operand (implied) */
    OP_DATA,      /* 8-bit immediate data literal */
    OP_ADDR_MEM,  /* 11-bit main-memory address */
    OP_ADDR_IO,   /* 11-bit I/O-memory address */
    OP_ADDR_PROG  /* 11-bit program-memory address (branch targets) */
} OperandType;

/* ------------------------------------------------------------------ */
/*  Instruction descriptor                                             */
/* ------------------------------------------------------------------ */
typedef struct {
    const char  *mnemonic;
    unsigned int opcode;      /* 5-bit opcode value */
    OperandType  operand_type;
} InstrDesc;

/*
 * Complete TRACS instruction set table.
 * Source: enum inst_opcode in Team-3-LE6_CPU-Memory-IO.c,
 *         confirmed against ISA slides.
 */
static const InstrDesc ISA[] = {
    /* Mnemonic    Opcode   Operand type        */
    { "WM",   0x01, OP_ADDR_MEM  },
    { "RM",   0x02, OP_ADDR_MEM  },
    { "BR",   0x03, OP_ADDR_PROG },
    { "RIO",  0x04, OP_ADDR_IO   },
    { "WIO",  0x05, OP_ADDR_IO   },
    { "WB",   0x06, OP_DATA      },
    { "WIB",  0x07, OP_DATA      },
    { "WACC", 0x09, OP_NONE      },
    { "RACC", 0x0B, OP_NONE      },
    { "SWAP", 0x0E, OP_NONE      },
    { "BRLT", 0x11, OP_ADDR_PROG },
    { "BRGT", 0x12, OP_ADDR_PROG },
    { "BRNE", 0x13, OP_ADDR_PROG },
    { "BRE",  0x14, OP_ADDR_PROG },
    { "SHR",  0x15, OP_NONE      },
    { "SHL",  0x16, OP_NONE      },
    { "XOR",  0x17, OP_NONE      },
    { "NOT",  0x18, OP_NONE      },
    { "OR",   0x19, OP_NONE      },
    { "AND",  0x1A, OP_NONE      },
    { "MUL",  0x1B, OP_NONE      },
    { "SUB",  0x1D, OP_NONE      },
    { "ADD",  0x1E, OP_NONE      },
    { "EOP",  0x1F, OP_NONE      }
};
#define ISA_SIZE ((int)(sizeof(ISA) / sizeof(ISA[0])))

/* ------------------------------------------------------------------ */
/*  Symbol table                                                       */
/* ------------------------------------------------------------------ */
typedef struct {
    char         name[MAX_LABEL_LEN];
    unsigned int address;  /* byte address in program memory */
} Symbol;

static Symbol  symtab[MAX_LABELS];
static int     sym_count = 0;

/* ------------------------------------------------------------------ */
/*  Error tracking                                                     */
/* ------------------------------------------------------------------ */
static int error_count  = 0;
static int eop_found    = 0;  /* 1 if EOP directive was seen */

/* ------------------------------------------------------------------ */
/*  Helper: convert mnemonic string to uppercase in place              */
/* ------------------------------------------------------------------ */
static void str_toupper(char *s) {
    while (*s) { *s = (char)toupper((unsigned char)*s); s++; }
}

/* ------------------------------------------------------------------ */
/*  Helper: strip leading/trailing whitespace, return pointer          */
/* ------------------------------------------------------------------ */
static char *trim(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) { *end = '\0'; end--; }
    return s;
}

/* ------------------------------------------------------------------ */
/*  Helper: look up mnemonic in ISA table; returns pointer or NULL     */
/* ------------------------------------------------------------------ */
static const InstrDesc *lookup_mnemonic(const char *mnem) {
    int i;
    for (i = 0; i < ISA_SIZE; i++) {
        if (strcmp(ISA[i].mnemonic, mnem) == 0)
            return &ISA[i];
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/*  Helper: look up a label in the symbol table                        */
/*  Returns address (unsigned int) via out_addr; returns 1 if found.  */
/* ------------------------------------------------------------------ */
static int lookup_label(const char *name, unsigned int *out_addr) {
    int i;
    for (i = 0; i < sym_count; i++) {
        if (strcmp(symtab[i].name, name) == 0) {
            *out_addr = symtab[i].address;
            return 1;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Helper: add a label to the symbol table                            */
/* ------------------------------------------------------------------ */
static void add_label(const char *name, unsigned int address) {
    if (sym_count >= MAX_LABELS) {
        fprintf(stderr, "ERROR: Symbol table overflow (max %d labels)\n", MAX_LABELS);
        return;
    }
    /* Check for duplicate */
    unsigned int dummy;
    if (lookup_label(name, &dummy)) {
        fprintf(stderr, "ERROR: Duplicate label definition '%s'\n", name);
        error_count++;
        return;
    }
    strncpy(symtab[sym_count].name, name, MAX_LABEL_LEN - 1);
    symtab[sym_count].name[MAX_LABEL_LEN - 1] = '\0';
    symtab[sym_count].address = address;
    sym_count++;
}

/* ------------------------------------------------------------------ */
/*  Helper: parse a numeric operand string (0x..., 0..., or decimal)  */
/*  Returns 1 on success, 0 on failure.                               */
/* ------------------------------------------------------------------ */
static int parse_number(const char *s, unsigned int *out_val) {
    char *endptr;
    long v;
    /* Handle hex prefix 0x / 0X */
    if ((s[0] == '0') && (s[1] == 'x' || s[1] == 'X')) {
        errno = 0;
        v = strtol(s, &endptr, 16);
    } else {
        errno = 0;
        v = strtol(s, &endptr, 0);
    }
    if (errno != 0 || endptr == s || *endptr != '\0') return 0;
    if (v < 0) return 0;
    *out_val = (unsigned int)v;
    return 1;
}

/* ------------------------------------------------------------------ */
/*  Helper: derive output filename from input name (replace/add .txt) */
/* ------------------------------------------------------------------ */
static void make_output_name(const char *infile, char *outfile, size_t outsize) {
    const char *dot = strrchr(infile, '.');
    size_t base_len = dot ? (size_t)(dot - infile) : strlen(infile);
    if (base_len >= outsize - 5) base_len = outsize - 5;
    strncpy(outfile, infile, base_len);
    outfile[base_len] = '\0';
    strncat(outfile, ".txt", outsize - base_len - 1);
}

/* ------------------------------------------------------------------ */
/*  Helper: check whether a string is a valid label token             */
/*  Labels must start with a letter or underscore and contain only    */
/*  alphanumeric characters or underscores.                           */
/* ------------------------------------------------------------------ */
static int is_valid_label(const char *s) {
    if (!s || !*s) return 0;
    if (!isalpha((unsigned char)s[0]) && s[0] != '_') return 0;
    const char *p = s + 1;
    while (*p) {
        if (!isalnum((unsigned char)*p) && *p != '_') return 0;
        p++;
    }
    /* Must not be a mnemonic */
    char upper[MAX_LABEL_LEN];
    strncpy(upper, s, MAX_LABEL_LEN - 1);
    upper[MAX_LABEL_LEN - 1] = '\0';
    str_toupper(upper);
    if (lookup_mnemonic(upper) != NULL) return 0;
    if (strcmp(upper, "ORG") == 0)      return 0;
    return 1;
}

/* ================================================================== */
/*                                                                    */
/*  PASS 1 : Build Symbol Table                                        */
/*                                                                    */
/*  Purpose: Find every label and the address it points to.           */
/*  (Flowchart: START of Pass 1 → read lines → ORG → labels →        */
/*              advance address counter by 2 → END of Pass 1)        */
/*                                                                    */
/* ================================================================== */
static int pass1(FILE *fp) {
    char         line[MAX_LINE_LEN];
    char         buf[MAX_LINE_LEN];
    unsigned int address_counter = 0x000u;  /* current byte address */
    int          org_seen        = 0;

    rewind(fp);

    while (fgets(line, sizeof(line), fp) != NULL) {

        /* ---- Strip comment (semicolon delimited) ---- */
        char *comment = strchr(line, ';');
        if (comment) *comment = '\0';

        /* Work on a copy */
        strncpy(buf, line, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        char *p = trim(buf);

        if (*p == '\0') continue;  /* blank / comment-only line */

        /* ---- Check for ORG directive ---- */
        /* Format: [label]  ORG  <address> */
        {
            char tok1[MAX_LABEL_LEN], tok2[MAX_MNEMONIC_LEN], tok3[MAX_OPERAND_LEN];
            int n = sscanf(p, "%63s %15s %31s", tok1, tok2, tok3);

            str_toupper(tok2);

            /* Case: "ORG <addr>" with no label */
            if (strcmp(tok1, "ORG") == 0 || (n >= 1 && (str_toupper(tok1), strcmp(tok1, "ORG") == 0))) {
                /* tok1 IS the ORG keyword (after upper-case) */
                if (n >= 2) {
                    unsigned int org_val = 0;
                    /* tok2 holds the address */
                    char tok2_up[MAX_MNEMONIC_LEN];
                    strncpy(tok2_up, tok2, MAX_MNEMONIC_LEN - 1);
                    tok2_up[MAX_MNEMONIC_LEN - 1] = '\0';
                    str_toupper(tok2_up);
                    if (!parse_number(tok2, &org_val)) {
                        fprintf(stderr, "ERROR: Invalid ORG operand '%s'\n", tok2);
                        error_count++;
                    } else {
                        address_counter = org_val;
                        org_seen = 1;
                    }
                }
                continue;
            }

            /* Re-read: format may be "label  ORG  <addr>" or
             *                         "label  mnemonic  [operand]"   */
            /* Reset tok2/tok3 first */
            tok2[0] = '\0'; tok3[0] = '\0';
            n = sscanf(p, "%63s %15s %31s", tok1, tok2, tok3);

            /* Upper-case tok2 for directive comparison */
            char tok2_up[MAX_MNEMONIC_LEN];
            strncpy(tok2_up, tok2, MAX_MNEMONIC_LEN - 1);
            tok2_up[MAX_MNEMONIC_LEN - 1] = '\0';
            str_toupper(tok2_up);

            /* ---- Flowchart: Is it the ORG directive? ---- */
            if (strcmp(tok2_up, "ORG") == 0) {
                /* tok1 is a label (attached to ORG line, unusual but handled) */
                /* tok3 is the address value */
                unsigned int org_val = 0;
                if (n >= 3 && parse_number(tok3, &org_val)) {
                    address_counter = org_val;
                    org_seen = 1;
                    /* label on ORG line: assign it the new address */
                    if (is_valid_label(tok1)) {
                        add_label(tok1, address_counter);
                    }
                } else {
                    fprintf(stderr, "ERROR: Invalid ORG operand '%s'\n", tok3);
                    error_count++;
                }
                continue;
            }

            if (!org_seen) {
                /* Lines before ORG: if they look like instructions we still
                 * need to count addresses from 0. This is spec-ambiguous;
                 * we start from 0x000 and flag no ORG in the report. */
                /* Fall through to label/instruction handling */
            }

            /* ---- Flowchart: Does the line have a label? ---- */
            /* A label appears as the first token if it is not a mnemonic.
             * Resolve: if tok1 is a known mnemonic (or ORG), no label;
             *          otherwise tok1 is a label, tok2 is the mnemonic. */
            char tok1_up[MAX_LABEL_LEN];
            strncpy(tok1_up, tok1, MAX_LABEL_LEN - 1);
            tok1_up[MAX_LABEL_LEN - 1] = '\0';
            str_toupper(tok1_up);

            int has_label     = 0;
            const char *mnem  = NULL;

            if (lookup_mnemonic(tok1_up) != NULL) {
                /* tok1 is a mnemonic, no label on this line */
                mnem = tok1_up;  /* mnemonic = tok1 */
            } else if (is_valid_label(tok1)) {
                /* tok1 is a label */
                has_label = 1;
                mnem      = tok2_up; /* mnemonic is tok2 */
            } else {
                /* Neither a known mnemonic nor a valid label — skip silently.
                 * (Could be an empty line that slipped through, etc.) */
                continue;
            }

            /* ---- Save label and its current address ---- */
            if (has_label) {
                add_label(tok1, address_counter);
            }

            /* ---- Advance address counter by 2 (next instruction) ---- */
            /* Only real instructions consume address space, not ORG.     */
            if (mnem && *mnem != '\0' && lookup_mnemonic(mnem) != NULL) {
                address_counter += 2;
            }
        }
    }

    return 0;
}

/* ================================================================== */
/*                                                                    */
/*  ERROR DETECTION (called inside pass2, per flowchart)              */
/*                                                                    */
/*  Checks (in order per flowchart):                                  */
/*  1. EOP found in source?                                           */
/*  2. Is the mnemonic in the instruction set?                        */
/*  3. Is the operand correct?                                        */
/*  4. Is the address in the valid range?                             */
/*                                                                    */
/* ================================================================== */

/* Validate a single instruction line.
 * mnem  : upper-case mnemonic string
 * op_str: raw operand string (empty if absent)
 * line_no: for error messages
 * Returns the InstrDesc* if valid, NULL if mnemonic unknown.
 * Sets error_count on any error. */
static const InstrDesc *validate_instruction(
        const char *mnem,
        const char *op_str,
        int         line_no)
{
    /* ---- Check 2: Is the mnemonic in the instruction set? ---- */
    const InstrDesc *desc = lookup_mnemonic(mnem);
    if (desc == NULL) {
        fprintf(stderr, "ERROR line %d: Unknown instruction '%s'\n", line_no, mnem);
        error_count++;
        return NULL;
    }

    /* Track EOP */
    if (desc->opcode == 0x1F) eop_found = 1;

    /* ---- Check 3: Is the operand correct? ---- */
    int op_present = (op_str != NULL && op_str[0] != '\0');

    if (desc->operand_type == OP_NONE) {
        /* Instruction should have NO operand */
        if (op_present) {
            fprintf(stderr,
                "ERROR line %d: Illegal operand for '%s' (expects none, got '%s')\n",
                line_no, mnem, op_str);
            error_count++;
            return desc;  /* still return for address counting */
        }
    } else {
        /* Instruction must have an operand */
        if (!op_present) {
            fprintf(stderr,
                "ERROR line %d: Missing operand for '%s'\n",
                line_no, mnem);
            error_count++;
            return desc;
        }

        /* Try to parse as a number */
        unsigned int val = 0;
        int is_num = parse_number(op_str, &val);
        if (!is_num) {
            /* Might be a label – label resolution happens in pass2.
             * Here we just check if it at least looks like a label. */
            if (!is_valid_label(op_str)) {
                fprintf(stderr,
                    "ERROR line %d: Invalid operand '%s' for '%s'\n",
                    line_no, op_str, mnem);
                error_count++;
                return desc;
            }
            /* Label: address range check deferred to pass2 */
            return desc;
        }

        /* ---- Check 4: Is the address in the valid range? ---- */
        switch (desc->operand_type) {
            case OP_DATA:
                if (val > 0xFF) {
                    fprintf(stderr,
                        "ERROR line %d: Data operand 0x%X out of range [0x00..0xFF] for '%s'\n",
                        line_no, val, mnem);
                    error_count++;
                }
                break;
            case OP_ADDR_MEM:
                if (val < DATA_MEM_MIN || val > DATA_MEM_MAX) {
                    fprintf(stderr,
                        "ERROR line %d: Memory address 0x%03X out of data range "
                        "[0x400..0x7FF] for '%s'\n",
                        line_no, val, mnem);
                    error_count++;
                }
                break;
            case OP_ADDR_IO:
                if (val > IO_MEM_MAX) {
                    fprintf(stderr,
                        "ERROR line %d: I/O address 0x%03X out of range "
                        "[0x000..0x01F] for '%s'\n",
                        line_no, val, mnem);
                    error_count++;
                }
                break;
            case OP_ADDR_PROG:
                if (val > PROG_MEM_MAX) {
                    fprintf(stderr,
                        "ERROR line %d: Program address 0x%03X out of range "
                        "[0x000..0x3FF] for '%s'\n",
                        line_no, val, mnem);
                    error_count++;
                }
                break;
            default:
                break;
        }
    }

    return desc;
}

/* ================================================================== */
/*                                                                    */
/*  PASS 2 : Translate Instructions to Machine Code                   */
/*                                                                    */
/*  Purpose: Re-scan source, look up labels, encode each instruction, */
/*           write big-endian 16-bit words to .txt output.            */
/*  (Flowchart: START of Pass 2 → go back to start → read next →     */
/*              label substitution → validate → encode → write →      */
/*              advance address → END of Pass 2)                      */
/*                                                                    */
/* ================================================================== */
static int pass2(FILE *fp, FILE *out) {
    char         line[MAX_LINE_LEN];
    char         buf[MAX_LINE_LEN];
    unsigned int address_counter = 0x000u;
    int          line_no         = 0;

    rewind(fp);

    /* ---- Flowchart: Go back to start of file ---- */
    while (fgets(line, sizeof(line), fp) != NULL) {
        line_no++;

        /* ---- Strip comment ---- */
        char *comment = strchr(line, ';');
        if (comment) *comment = '\0';

        strncpy(buf, line, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        char *p = trim(buf);

        if (*p == '\0') continue;  /* blank or comment-only */

        /* ---- Read next instruction line ---- */
        char tok1[MAX_LABEL_LEN];
        char tok2[MAX_MNEMONIC_LEN];
        char tok3[MAX_OPERAND_LEN];
        tok1[0] = tok2[0] = tok3[0] = '\0';

        int n = sscanf(p, "%63s %15s %31s", tok1, tok2, tok3);
        (void)n;

        /* Upper-case first token to detect ORG / mnemonic */
        char tok1_up[MAX_LABEL_LEN];
        strncpy(tok1_up, tok1, MAX_LABEL_LEN - 1);
        tok1_up[MAX_LABEL_LEN - 1] = '\0';
        str_toupper(tok1_up);

        /* ---- Handle ORG directive ---- */
        if (strcmp(tok1_up, "ORG") == 0) {
            unsigned int org_val = 0;
            if (tok2[0] != '\0' && parse_number(tok2, &org_val)) {
                address_counter = org_val;
            }
            continue;
        }

        /* Upper-case tok2 */
        char tok2_up[MAX_MNEMONIC_LEN];
        strncpy(tok2_up, tok2, MAX_MNEMONIC_LEN - 1);
        tok2_up[MAX_MNEMONIC_LEN - 1] = '\0';
        str_toupper(tok2_up);

        /* Check if tok1 itself is a mnemonic or ORG on a label'd ORG line */
        if (strcmp(tok2_up, "ORG") == 0) {
            /* "label  ORG  <addr>" */
            unsigned int org_val = 0;
            if (tok3[0] != '\0' && parse_number(tok3, &org_val)) {
                address_counter = org_val;
            }
            continue;
        }

        /* Determine mnemonic and operand string */
        const char  *mnem_str;
        const char  *op_str;
        char         mnem_up[MAX_MNEMONIC_LEN];

        if (lookup_mnemonic(tok1_up) != NULL) {
            /* tok1 is the mnemonic */
            strncpy(mnem_up, tok1_up, MAX_MNEMONIC_LEN - 1);
            mnem_up[MAX_MNEMONIC_LEN - 1] = '\0';
            mnem_str = mnem_up;
            op_str   = (tok2[0] != '\0') ? tok2 : "";
        } else if (is_valid_label(tok1)) {
            /* tok1 is a label, tok2 is the mnemonic */
            strncpy(mnem_up, tok2_up, MAX_MNEMONIC_LEN - 1);
            mnem_up[MAX_MNEMONIC_LEN - 1] = '\0';
            mnem_str = mnem_up;
            op_str   = (tok3[0] != '\0') ? tok3 : "";
            /* If tok2 is also not a known mnemonic, it's an unknown instruction */
        } else {
            /* tok1 is neither a mnemonic nor a valid label — unknown instruction */
            fprintf(stderr,
                "ERROR line %d: Unknown instruction '%s'\n",
                line_no, tok1);
            error_count++;
            address_counter += 2;
            continue;
        }

        if (mnem_str[0] == '\0') continue;

        /* ---- Flowchart: Does instruction contain a label? ---- */
        /* If the operand is not a number, attempt label lookup.   */
        char resolved_op[MAX_OPERAND_LEN];
        strncpy(resolved_op, op_str, MAX_OPERAND_LEN - 1);
        resolved_op[MAX_OPERAND_LEN - 1] = '\0';

        if (op_str[0] != '\0') {
            unsigned int dummy = 0;
            if (!parse_number(op_str, &dummy)) {
                /* ---- Flowchart: Substitute label with saved address ---- */
                unsigned int label_addr = 0;
                if (lookup_label(op_str, &label_addr)) {
                    snprintf(resolved_op, MAX_OPERAND_LEN, "0x%03X", label_addr);
                } else {
                    fprintf(stderr,
                        "ERROR line %d: Undefined label '%s'\n",
                        line_no, op_str);
                    error_count++;
                    /* Advance counter and continue */
                    address_counter += 2;
                    continue;
                }
            }
        }

        /* ---- Flowchart: Are the mnemonic and operand valid? ---- */
        const InstrDesc *desc = validate_instruction(mnem_str, resolved_op, line_no);

        if (desc == NULL) {
            /* ---- Flowchart: Record error ---- */
            address_counter += 2;  /* still advance so subsequent labels are correct */
            continue;
        }

        /* ---- Flowchart: Convert to machine code ---- */
        unsigned int operand_val = 0;
        if (desc->operand_type != OP_NONE && resolved_op[0] != '\0') {
            parse_number(resolved_op, &operand_val);
            /* Mask to appropriate width */
            if (desc->operand_type == OP_DATA)
                operand_val &= 0xFF;
            else
                operand_val &= 0x7FF;
        }

        /* 16-bit instruction word: opcode(5) | operand(11) */
        unsigned int word = ((desc->opcode & 0x1F) << 11) | (operand_val & 0x07FF);
        unsigned int high_byte = (word >> 8) & 0xFF;
        unsigned int low_byte  =  word       & 0xFF;

        /* ---- Flowchart: Write to .txt ---- */
        fprintf(out, "ADDR=0x%03X; BUS=0x%02X; MainMemory();\n",
                address_counter,     high_byte);
        fprintf(out, "ADDR=0x%03X; BUS=0x%02X; MainMemory();\n",
                address_counter + 1, low_byte);

        /* ---- Flowchart: Advance address ---- */
        address_counter += 2;
    }

    return 0;
}

/* ================================================================== */
/*                                                                    */
/*  MAIN ENTRY POINT                                                   */
/*                                                                    */
/*  Flowchart (top level):                                            */
/*    START → Read .asm source file →                                 */
/*      Pass 1: Scan source and build symbol table →                  */
/*      Pass 2: Translate instructions to machine code →              */
/*      No errors detected? → Yes: Report build success               */
/*                          → No:  Report errors to console           */
/*    END                                                             */
/*                                                                    */
/* ================================================================== */
int main(int argc, char *argv[]) {
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: %s <input.asm> [output.txt]\n", argv[0]);
        return 1;
    }

    const char *infile_name = argv[1];
    char        outfile_name[512];

    if (argc == 3) {
        strncpy(outfile_name, argv[2], sizeof(outfile_name) - 1);
        outfile_name[sizeof(outfile_name) - 1] = '\0';
    } else {
        make_output_name(infile_name, outfile_name, sizeof(outfile_name));
    }

    /* ---- Flowchart: Read .asm source file ---- */
    FILE *fp = fopen(infile_name, "r");
    if (!fp) {
        fprintf(stderr, "ERROR: Cannot open input file '%s': %s\n",
                infile_name, strerror(errno));
        return 1;
    }

    printf("TRACS Assembler\n");
    printf("Input  : %s\n", infile_name);
    printf("Output : %s\n\n", outfile_name);

    /* ---- Flowchart: Pass 1 – Scan source and build symbol table ---- */
    printf("[Pass 1] Building symbol table...\n");
    pass1(fp);

    if (sym_count > 0) {
        printf("  Labels found: %d\n", sym_count);
        int i;
        for (i = 0; i < sym_count; i++) {
            printf("    %-20s  =>  0x%03X\n",
                   symtab[i].name, symtab[i].address);
        }
    } else {
        printf("  No labels defined.\n");
    }

    /* ---- Flowchart: Pass 2 – Translate instructions to machine code ---- */
    printf("\n[Pass 2] Translating to machine code...\n");

    FILE *out = fopen(outfile_name, "w");
    if (!out) {
        fprintf(stderr, "ERROR: Cannot create output file '%s': %s\n",
                outfile_name, strerror(errno));
        fclose(fp);
        return 1;
    }

    pass2(fp, out);

    fclose(out);
    fclose(fp);

    /* ---- Flowchart: Check 1 – EOP found in source? ---- */
    if (!eop_found) {
        fprintf(stderr, "ERROR: No EOP instruction found in source\n");
        error_count++;
    }

    /* ---- Flowchart: No errors detected? ---- */
    if (error_count == 0) {
        /* ---- Flowchart: Report build success ---- */
        printf("\nBuild successful. Output written to '%s'.\n", outfile_name);
    } else {
        /* ---- Flowchart: Report errors to console ---- */
        fprintf(stderr, "\nBuild FAILED with %d error(s). No output file produced.\n",
                error_count);
        /* Remove partial output */
        remove(outfile_name);
        return 1;
    }

    return 0;
}
