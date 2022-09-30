/* C code produced by gperf version 3.0.4 */
/* Command-line: /usr/bin/gperf -m 10 -C -G -D -t  */
/* Computed positions: -k'2-3,6,$' */

#if !((' ' == 32) && ('!' == 33) && ('"' == 34) && ('#' == 35) \
      && ('%' == 37) && ('&' == 38) && ('\'' == 39) && ('(' == 40) \
      && (')' == 41) && ('*' == 42) && ('+' == 43) && (',' == 44) \
      && ('-' == 45) && ('.' == 46) && ('/' == 47) && ('0' == 48) \
      && ('1' == 49) && ('2' == 50) && ('3' == 51) && ('4' == 52) \
      && ('5' == 53) && ('6' == 54) && ('7' == 55) && ('8' == 56) \
      && ('9' == 57) && (':' == 58) && (';' == 59) && ('<' == 60) \
      && ('=' == 61) && ('>' == 62) && ('?' == 63) && ('A' == 65) \
      && ('B' == 66) && ('C' == 67) && ('D' == 68) && ('E' == 69) \
      && ('F' == 70) && ('G' == 71) && ('H' == 72) && ('I' == 73) \
      && ('J' == 74) && ('K' == 75) && ('L' == 76) && ('M' == 77) \
      && ('N' == 78) && ('O' == 79) && ('P' == 80) && ('Q' == 81) \
      && ('R' == 82) && ('S' == 83) && ('T' == 84) && ('U' == 85) \
      && ('V' == 86) && ('W' == 87) && ('X' == 88) && ('Y' == 89) \
      && ('Z' == 90) && ('[' == 91) && ('\\' == 92) && (']' == 93) \
      && ('^' == 94) && ('_' == 95) && ('a' == 97) && ('b' == 98) \
      && ('c' == 99) && ('d' == 100) && ('e' == 101) && ('f' == 102) \
      && ('g' == 103) && ('h' == 104) && ('i' == 105) && ('j' == 106) \
      && ('k' == 107) && ('l' == 108) && ('m' == 109) && ('n' == 110) \
      && ('o' == 111) && ('p' == 112) && ('q' == 113) && ('r' == 114) \
      && ('s' == 115) && ('t' == 116) && ('u' == 117) && ('v' == 118) \
      && ('w' == 119) && ('x' == 120) && ('y' == 121) && ('z' == 122) \
      && ('{' == 123) && ('|' == 124) && ('}' == 125) && ('~' == 126))
/* The character set is not based on ISO-646.  */
error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gnu-gperf@gnu.org>."
#endif


/*
 * Copyright (C) 2005 Andreas Steffen
 * HSR Hochschule fuer Technik Rapperswil
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/gpl.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#include <string.h>

#define IN_GPERF_GENERATED_FILE
#include "keywords.h"

struct kw_entry {
    char *name;
    kw_token_t token;
};

#define TOTAL_KEYWORDS 150
#define MIN_WORD_LENGTH 2
#define MAX_WORD_LENGTH 17
#define MIN_HASH_VALUE 24
#define MAX_HASH_VALUE 343
/* maximum key range = 320, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
hash (str, len)
     register const char *str;
     register unsigned int len;
{
  static const unsigned short asso_values[] =
    {
      344, 344, 344, 344, 344, 344, 344, 344, 344, 344,
      344, 344, 344, 344, 344, 344, 344, 344, 344, 344,
      344, 344, 344, 344, 344, 344, 344, 344, 344, 344,
      344, 344, 344, 344, 344, 344, 344, 344, 344, 344,
      344, 344, 344, 344, 344, 344, 344, 344, 344,  20,
      110, 344, 344, 344, 344, 344, 344, 344, 344, 344,
      344, 344, 344, 344, 344, 344, 344, 344, 344, 344,
      344, 344, 344, 344, 344, 344, 344, 344, 344, 344,
      344, 344, 344, 344, 344, 344, 344, 344, 344, 344,
      344, 344, 344, 344, 344,   4, 344,  44, 344,  65,
       65,   4,  66,  99, 120,   4, 344, 141,   4, 104,
       29,  65,  27, 344,   8,  18,  13, 158,   8, 344,
        8,  14,   4, 344, 344, 344, 344, 344, 344, 344,
      344, 344, 344, 344, 344, 344, 344, 344, 344, 344,
      344, 344, 344, 344, 344, 344, 344, 344, 344, 344,
      344, 344, 344, 344, 344, 344, 344, 344, 344, 344,
      344, 344, 344, 344, 344, 344, 344, 344, 344, 344,
      344, 344, 344, 344, 344, 344, 344, 344, 344, 344,
      344, 344, 344, 344, 344, 344, 344, 344, 344, 344,
      344, 344, 344, 344, 344, 344, 344, 344, 344, 344,
      344, 344, 344, 344, 344, 344, 344, 344, 344, 344,
      344, 344, 344, 344, 344, 344, 344, 344, 344, 344,
      344, 344, 344, 344, 344, 344, 344, 344, 344, 344,
      344, 344, 344, 344, 344, 344, 344, 344, 344, 344,
      344, 344, 344, 344, 344, 344, 344, 344, 344, 344,
      344, 344, 344, 344, 344, 344
    };
  register int hval = len;

  switch (hval)
    {
      default:
        hval += asso_values[(unsigned char)str[5]];
      /*FALLTHROUGH*/
      case 5:
      case 4:
      case 3:
      case 2:
        hval += asso_values[(unsigned char)str[1]];
      /*FALLTHROUGH*/
      case 1:
        hval += asso_values[(unsigned char)str[0]];
        break;
    }
  return hval + asso_values[(unsigned char)str[len - 1]];
}

static const struct kw_entry wordlist[] =
  {
    {"lifetime",          KW_KEYLIFE},
    {"left",              KW_LEFT},
    {"leftfirewall",      KW_LEFTFIREWALL},
    {"rightimei",         KW_RIGHTIMEI},
    {"right",             KW_RIGHT},
    {"rekey",             KW_REKEY},
    {"leftcert",          KW_LEFTCERT},
    {"type",              KW_TYPE},
    {"leftsigkey",        KW_LEFTSIGKEY},
    {"leftsendcert",      KW_LEFTSENDCERT},
    {"leftallowany",      KW_LEFTALLOWANY},
    {"leftcertpolicy",    KW_LEFTCERTPOLICY},
    {"rightikeport",      KW_RIGHTIKEPORT},
    {"leftprotoport",     KW_LEFTPROTOPORT},
    {"rightintsubnet",    KW_RIGHTINTSUBNET},
    {"leftgroups",        KW_LEFTGROUPS},
    {"rightrsasigkey",    KW_RIGHTSIGKEY},
    {"lifebytes",         KW_LIFEBYTES},
    {"leftnexthop",       KW_LEFT_DEPRECATED},
    {"esp",               KW_ESP},
    {"leftrsasigkey",     KW_LEFTSIGKEY},
    {"rightsubnet",       KW_RIGHTSUBNET},
    {"rightsigkey",       KW_RIGHTSIGKEY},
    {"rightsendcert",     KW_RIGHTSENDCERT},
    {"rightidr_apn",      KW_RIGHTIDR_APN},
    {"inactivity",        KW_INACTIVITY},
    {"leftdns",           KW_LEFTDNS},
    {"leftintsubnet",     KW_LEFTINTSUBNET},
    {"installpolicy",     KW_INSTALLPOLICY},
    {"rightprotoport",    KW_RIGHTPROTOPORT},
    {"rightsourceip",     KW_RIGHTSOURCEIP},
    {"plutostart",        KW_SETUP_DEPRECATED},
    {"strictcrlpolicy",   KW_STRICTCRLPOLICY},
    {"leftupdown",        KW_LEFTUPDOWN},
    {"virtual_private",   KW_SETUP_DEPRECATED},
    {"rightsubnetwithin", KW_RIGHTSUBNET},
    {"eap",               KW_CONN_DEPRECATED},
    {"rightnexthop",      KW_RIGHT_DEPRECATED},
    {"lifepackets",       KW_LIFEPACKETS},
    {"reqid",             KW_REQID},
    {"rightallowany",     KW_RIGHTALLOWANY},
    {"xauth_identity",    KW_XAUTH_IDENTITY},
    {"crluri",            KW_CRLURI},
    {"rightid",           KW_RIGHTID},
    {"rekeyfuzz",         KW_REKEYFUZZ},
    {"certuribase",       KW_CERTURIBASE},
    {"rightfirewall",     KW_RIGHTFIREWALL},
    {"crlcheckinterval",  KW_SETUP_DEPRECATED},
    {"nat_traversal",     KW_SETUP_DEPRECATED},
    {"rightcert",         KW_RIGHTCERT},
    {"leftca",            KW_LEFTCA},
    {"rightdns",          KW_RIGHTDNS},
    {"crluri1",           KW_CRLURI},
    {"lefthostaccess",    KW_LEFTHOSTACCESS},
    {"rightcertpolicy",   KW_RIGHTCERTPOLICY},
    {"rightsourceif",     KW_RIGHTSOURCEIF},
    {"packetdefault",     KW_SETUP_DEPRECATED},
    {"leftsourceip",      KW_LEFTSOURCEIP},
    {"leftidr_apn",       KW_LEFTIDR_APN},
    {"pfs",               KW_PFS_DEPRECATED},
    {"rightpcscf",        KW_RIGHTPCSCF},
    {"also",              KW_ALSO},
    {"dpddelay",          KW_DPDDELAY},
    {"fragmentation",     KW_FRAGMENTATION},
    {"leftimei",          KW_LEFTIMEI},
    {"ldapbase",          KW_CA_DEPRECATED},
    {"interfaces",        KW_SETUP_DEPRECATED},
    {"rightca",           KW_RIGHTCA},
    {"leftcert2",         KW_LEFTCERT2},
    {"rightid2",          KW_RIGHTID2},
    {"leftgroups2",       KW_LEFTGROUPS2},
    {"eap_identity",      KW_EAP_IDENTITY},
    {"rightgroups",       KW_RIGHTGROUPS},
    {"cacert",            KW_CACERT},
    {"dpdaction",         KW_DPDACTION},
    {"leftid",            KW_LEFTID},
    {"mediated_by",       KW_MEDIATED_BY},
    {"tfc",               KW_TFC},
    {"leftpcscf",         KW_LEFTPCSCF},
    {"ocspuri",           KW_OCSPURI},
    {"leftsourceif",      KW_LEFTSOURCEIF},
    {"ike",               KW_IKE},
    {"closeaction",       KW_CLOSEACTION},
    {"force_keepalive",   KW_SETUP_DEPRECATED},
    {"ldaphost",          KW_CA_DEPRECATED},
    {"rekeymargin",       KW_REKEYMARGIN},
    {"mediation",         KW_MEDIATION},
    {"compress",          KW_COMPRESS},
    {"plutostderrlog",    KW_SETUP_DEPRECATED},
    {"forceencaps",       KW_FORCEENCAPS},
    {"righthostaccess",   KW_RIGHTHOSTACCESS},
    {"ocspuri1",          KW_OCSPURI},
    {"leftca2",           KW_LEFTCA2},
    {"postpluto",         KW_SETUP_DEPRECATED},
    {"nocrsend",          KW_SETUP_DEPRECATED},
    {"rightintnetmask",   KW_RIGHTINTNETMASK},
    {"leftikeport",       KW_LEFTIKEPORT},
    {"fragicmp",          KW_SETUP_DEPRECATED},
    {"aggressive",        KW_AGGRESSIVE},
    {"rightauth2",        KW_RIGHTAUTH2},
    {"xauth",             KW_XAUTH},
    {"aaa_identity",      KW_AAA_IDENTITY},
    {"mobike",	           KW_MOBIKE},
    {"rightauth",         KW_RIGHTAUTH},
    {"me_peerid",         KW_ME_PEERID},
    {"mark_in",           KW_MARK_IN},
    {"leftsubnet",        KW_LEFTSUBNET},
    {"leftid2",           KW_LEFTID2},
    {"margintime",        KW_REKEYMARGIN},
    {"leftintnetmask",	   KW_LEFTINTNETMASK},
    {"pfsgroup",          KW_PFS_DEPRECATED},
    {"crluri2",           KW_CRLURI2},
    {"rightca2",          KW_RIGHTCA2},
    {"rightcert2",        KW_RIGHTCERT2},
    {"cachecrls",         KW_CACHECRLS},
    {"keep_alive",        KW_SETUP_DEPRECATED},
    {"pkcs11module",      KW_PKCS11_DEPRECATED},
    {"plutodebug",        KW_SETUP_DEPRECATED},
    {"marginbytes",       KW_MARGINBYTES},
    {"pkcs11keepstate",   KW_PKCS11_DEPRECATED},
    {"marginpackets",     KW_MARGINPACKETS},
    {"rightupdown",       KW_RIGHTUPDOWN},
    {"leftsubnetwithin",  KW_LEFTSUBNET},
    {"pkcs11proxy",       KW_PKCS11_DEPRECATED},
    {"hidetos",           KW_SETUP_DEPRECATED},
    {"uniqueids",         KW_UNIQUEIDS},
    {"dpdtimeout",        KW_DPDTIMEOUT},
    {"pkcs11initargs",    KW_PKCS11_DEPRECATED},
    {"keylife",           KW_KEYLIFE},
    {"keyexchange",       KW_KEYEXCHANGE},
    {"ikelifetime",       KW_IKELIFETIME},
    {"rightgroups2",      KW_RIGHTGROUPS2},
    {"mark_out",          KW_MARK_OUT},
    {"authby",            KW_AUTHBY},
    {"charonstart",       KW_SETUP_DEPRECATED},
    {"dumpdir",           KW_SETUP_DEPRECATED},
    {"ikedscp",           KW_IKEDSCP,},
    {"overridemtu",       KW_SETUP_DEPRECATED},
    {"ocspuri2",          KW_OCSPURI2},
    {"reauth",            KW_REAUTH},
    {"prepluto",          KW_SETUP_DEPRECATED},
    {"auto",              KW_AUTO},
    {"keyingtries",       KW_KEYINGTRIES},
    {"leftauth2",         KW_LEFTAUTH2},
    {"ah",                KW_AH},
    {"mark",              KW_MARK},
    {"leftauth",          KW_LEFTAUTH},
    {"klipsdebug",        KW_SETUP_DEPRECATED},
    {"charondebug",       KW_CHARONDEBUG},
    {"modeconfig",        KW_MODECONFIG}
  };

static const short lookup[] =
  {
     -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
     -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
     -1,  -1,  -1,  -1,   0,   1,  -1,  -1,   2,   3,
      4,   5,  -1,   6,  -1,   7,   8,   9,  10,  -1,
     11,  12,  13,  14,  15,  -1,  -1,  -1,  16,  17,
     18,  -1,  19,  20,  21,  22,  23,  24,  -1,  -1,
     -1,  25,  26,  27,  28,  -1,  29,  -1,  -1,  -1,
     30,  -1,  31,  32,  33,  34,  35,  -1,  36,  -1,
     37,  38,  39,  40,  41,  -1,  -1,  42,  43,  -1,
     -1,  44,  45,  -1,  -1,  46,  -1,  47,  48,  49,
     -1,  -1,  50,  51,  52,  53,  54,  -1,  -1,  55,
     56,  -1,  57,  58,  59,  60,  -1,  61,  62,  -1,
     63,  -1,  -1,  -1,  64,  65,  -1,  66,  67,  -1,
     -1,  68,  -1,  -1,  69,  -1,  -1,  70,  -1,  71,
     72,  73,  -1,  74,  75,  -1,  76,  77,  78,  79,
     -1,  80,  81,  82,  83,  84,  85,  -1,  -1,  86,
     87,  -1,  88,  -1,  89,  90,  91,  -1,  -1,  92,
     93,  94,  95,  96,  97,  98,  99, 100,  -1, 101,
     -1,  -1,  -1, 102,  -1, 103, 104,  -1, 105, 106,
    107, 108, 109, 110, 111, 112,  -1, 113,  -1,  -1,
     -1, 114,  -1, 115, 116, 117, 118, 119, 120,  -1,
    121, 122,  -1, 123, 124,  -1,  -1,  -1, 125, 126,
    127,  -1, 128,  -1,  -1, 129, 130,  -1,  -1,  -1,
     -1,  -1,  -1, 131, 132,  -1, 133,  -1, 134,  -1,
     -1,  -1, 135,  -1, 136,  -1, 137,  -1,  -1,  -1,
     -1,  -1,  -1,  -1,  -1,  -1, 138,  -1, 139,  -1,
     -1,  -1,  -1,  -1,  -1,  -1, 140,  -1,  -1,  -1,
     -1, 141,  -1, 142,  -1,  -1,  -1,  -1,  -1,  -1,
     -1,  -1,  -1,  -1,  -1, 143, 144,  -1,  -1,  -1,
     -1,  -1,  -1, 145, 146,  -1,  -1,  -1,  -1,  -1,
     -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
     -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1, 147,
     -1,  -1,  -1,  -1, 148,  -1,  -1,  -1,  -1,  -1,
     -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
     -1,  -1,  -1, 149
  };

#ifdef __GNUC__
__inline
#if defined __GNUC_STDC_INLINE__ || defined __GNUC_GNU_INLINE__
__attribute__ ((__gnu_inline__))
#endif
#endif
const struct kw_entry *
in_word_set (str, len)
     register const char *str;
     register unsigned int len;
{
  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register int key = hash (str, len);

      if (key <= MAX_HASH_VALUE && key >= 0)
        {
          register int index = lookup[key];

          if (index >= 0)
            {
              register const char *s = wordlist[index].name;

              if (*str == *s && !strcmp (str + 1, s + 1))
                return &wordlist[index];
            }
        }
    }
  return 0;
}
