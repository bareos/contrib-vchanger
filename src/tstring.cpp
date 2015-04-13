/* tstring.cpp
 *
 *  This file is part of the msafe package by Josh Fisher.
 *
 *  vchanger copyright (C) 2006-2014 Josh Fisher
 *
 *  vchanger is free software.
 *  You may redistribute it and/or modify it under the terms of the
 *  GNU General Public License, version 2, as published by the Free
 *  Software Foundation
 *
 *  vchanger is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with msafe.  See the file "COPYING".  If not,
 *  write to:  The Free Software Foundation, Inc.,
 *             59 Temple Place - Suite 330,
 *             Boston,  MA  02111-1307, USA.
 */
#include "config.h"
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif
#ifdef HAVE_CTYPE_H
#include <ctype.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include "tstring.h"



/*
 *  Function to make string lower case
 */
tString& tToLower(tString &b)
{
   tStringIterator p;
   for (p = b.begin(); p != b.end(); p++) {
      *p = tolower(*p);
   }
   return b;
}

tString tToLower(const char *sin)
{
   tString b(sin);
   tToLower(b);
   return b;
}

/*
 *  Function to make string upper case
 */
tString& tToUpper(tString &b)
{
   tStringIterator p;
   for (p = b.begin(); p != b.end(); p++) {
      *p = toupper(*p);
   }
   return b;
}

tString tToUpper(const char *sin)
{
   tString b(sin);
   tToUpper(b);
   return b;
}

/*
 *  Function to strip leading whitespace
 */
tString& tStripLeft(tString &b)
{
   tStringIterator p;
   for (p = b.begin(); p != b.end() && isblank(*p); p++) ;
   if (p != b.begin()) {
      b.erase(b.begin(), p);
   }
   return b;
}

tString tStripLeft(const char *sin)
{
   tString b(sin);
   tStripLeft(b);
   return b;
}

/*
 *  Function to strip trailing blanks
 */
tString& tStripRight(tString &b)
{
   tStringIterator p = b.begin();
   size_t n = b.find_last_not_of(" \t");
   if (n != tString::npos) p += (n + 1);
   b.erase(p, b.end());
   return b;
}

tString tStripRight(const char *sin)
{
   tString b(sin);
   tStripRight(b);
   return b;
}

/*
 *  Function to strip leading and trailing whitespace
 */
tString& tStrip(tString &b)
{
   return tStripLeft(tStripRight(b));
}

tString tStrip(const char *sin)
{
   tString b(sin);
   tStrip(b);
   return b;
}

/*
 *  Function to remove all SP and HTAB chars from b
 */
tString& tRemoveWS(tString &b)
{
   size_t n = b.find_first_of(" \t");
   while (n != tString::npos) {
      b.erase(n, 1);
      n = b.find_first_of(" \t", n);
   }
   return b;
}

tString tRemoveWS(const char *sin)
{
   tString b(sin);
   tRemoveWS(b);
   return b;
}

/*
 *  Function to remove trailing line ending
 */
tString& tRemoveEOL(tString &b)
{
   if (b.empty() || b[b.size() - 1] != '\n') return b;
   b.erase(b.end() - 1);
   if (!b.empty() && b[b.size() - 1] == '\r') b.erase(b.end() - 1);
   return b;
}

tString tRemoveEOL(const char *sin)
{
   tString b(sin);
   tRemoveEOL(b);
   return b;
}

/*
 *  Function to change LF endings to CRLF
 */
tString& tLF2CRLF(tString &b)
{
   size_t n = b.find_first_of('\n');
   while (n != tString::npos) {
      if (n && b[n-1] != '\r') {
         b.insert(n, 1, '\r');
         n += 2;
      } else ++n;
      n = b.find_first_of('\n', n);
   }
   return b;
}

tString tLF2CRLF(const char *sin)
{
   tString b(sin);
   tLF2CRLF(b);
   return b;
}

/*
 *  Function to change CRLF endings to LF
 */
tString& tCRLF2LF(tString &b)
{
   size_t n = b.find("\r\n");
   while (n != tString::npos) {
      b.erase(n, 1);
      n = b.find("\r\n", n);
   }
   return b;
}

tString tCRLF2LF(const char *sin)
{
   tString b(sin);
   tCRLF2LF(b);
   return b;
}

/*
 *  Function to extract the next line of text from sin
 *  Returns line extracted.
 */
tString tExtractLine(const tString &b, size_t &pos)
{
   size_t p1 = pos;
   pos = b.find('\n', pos);
   if (pos == tString::npos) pos = b.size();
   else ++pos;
   return b.substr(p1, pos - p1);
}

tString tExtractLine(const char *sin, size_t &pos)
{
   tString b(sin);
   return tExtractLine(b, pos);
}

/*
 *  Function to compare two case-insensitive strings as in C function strcasecmp()
 */
int tCaseCmp(const char *a, const char *b)
{
   if (!a || !a[0]) return b && b[0] ? -1 : 0;
   if (!b || !b[0]) return 1;
   return strcasecmp(a, b);
}

/*
 *  Function to do case-insensitive search for character b from position pos
 *  in string a
 */
size_t tCaseFind(const tString &a, char b, size_t pos)
{
   b = tolower(b);
   tString str = a;
   tToLower(str);
   return str.find(b, pos);
}

/*
 *  Function to do case-insensitive search for C string b from position pos
 *  in string a
 */
size_t tCaseFind(const tString &a, const char *b_in, size_t pos)
{
   tString str(a), b(b_in);
   tToLower(str);
   tToLower(b);
   return str.find(b, pos);
}

/*
 *  Function to get line of text from a file stream
 */
const char* tGetLine(tString &str, FILE *FS)
{
   char buf[4096];
   if (fgets(buf, sizeof(buf), FS) == NULL) {
      str.clear();
      return NULL;
   }
   str = buf;
   return str.c_str();
}

/*
 *  Function to format a string using sprintf
 */
const char* tFormat(tString &str, const char *fmt, ...)
{
   char buf[8192];
   va_list vl;
   va_start(vl, fmt);
   vsnprintf(buf, sizeof(buf), fmt, vl);
   va_end(vl);
   buf[8191] = 0;
   str = buf;
   return str.c_str();
}

/*
 *  Function to parse next token from position 'pos' in string 'str'.
 *  The text determining the token is placed in 'word'. 'pos' is
 *  updated to reference the character following the token text, or
 *  end of string if no characters follow the token text.
 *  Returns:
 *    0        Past end of string
 *    'A'      Atom was found
 *    ' '      Whitespace was found
 *    '\n'     EOL char sequence was found
 *    <other>  Special char was found
 */
char tParseStandard(tString &word, const char *str, size_t &pos, tString special, tString ws)
{
   size_t n, slen;
   if (ws.empty()) ws = " \t";
   word.clear();
   if (!str || !str[0]) return 0;
   slen = strlen(str);
   if (pos >= slen) {
      pos = slen;
      return 0;
   }
   if (ws.find(str[pos]) != tString::npos) {
      /* Extract whitespace sequence */
      for (n = pos; n < slen && ws.find(str[n]) != tString::npos; n++) ;
      word.insert(0, str + pos, n - pos);
      pos = n;
      return ' ';
   }
   if (special.find(str[pos]) != tString::npos) {
      /* Extract special character */
      word = str[pos++];
      return word[0];
   }
   if (str[pos] == '\r') {
      /* Extract CR or CRLF line ending */
      word = '\r';
      ++pos;
      if (str[pos] == '\n') {
         word += '\n';
         ++pos;
      }
      return '\n';
   }
   if (str[pos] == '\n') {
      /* Extract LF line ending */
      word = '\n';
      ++pos;
      return '\n';
   }
   /* Extract atom */
   for (n = pos; n < slen && str[n] != '\r' && str[n] != '\n' && ws.find(str[n]) == tString::npos
         && special.find(str[n]) == tString::npos; n++) ;
   word.insert(0, str + pos, n - pos);
   pos = n;
   return 'A';
}


/*
 *  Function to parse next value in comma-separated-value string 'str'
 *  on or after position 'pos' and store the value in 'word'. The value
 *  returned in 'word' will have quoting and escape sequences decoded.
 *  'pos' is updated and set to the position in 'str' of the character
 *  following the next delimiter (',') or end of line/string if there are
 *  no more delimiters.
 *  Returns:
 *     -1    Missing closing quote
 *     0     Past end of string
 *     1     Found column field
 *     2     Found line ending
 */
int tParseCSV(tString &word, const char *str, size_t &pos)
{
   size_t p, slen = strlen(str);
   char c;
   tString w;

   word.clear();
   c = tParseStandard(w, str, pos, "\",");
   if (!c) return 0; /* At end of string */
   if (c == '\n') {
      /* EOL found */
      word = w;
      return 2;
   }
   /* Skip leading whitespace */
   while (c == ' ') c = tParseStandard(w, str, pos, "\",");
   if (c == 0) {
      /* Found empty field terminated by end of string */
      return 0;
   }
   if (c == '\n') {
      /* Found empty field terminated by EOL */
      word = w;
      return 2;
   }
   /* extract field */
   while (c && c != '\n' && c != ',') {
      if (c == '"') {
         /* Extracting quoted string */
         c = tParseStandard(w, str, pos, "\"\\");
         while (c && c != '"') {
            if (c == '\\') {
               switch (str[pos]) {
               case 0:
                  return -1;
               case 'r':
                  word += '\r';
                  break;
               case 'n':
                  word += '\n';
                  break;
               case 't':
                  word += '\t';
                  break;
               case 'v':
                  word += '\v';
                  break;
               case 'b':
                  word += '\b';
                  break;
               case 'a':
                  word += '\a';
                  break;
               case 'x':
                  /* Process hex escape sequence */
                  if (isxdigit(str[pos + 1])) {
                     ++pos;
                     w.clear();
                     for (p = 0; pos < slen && p < 2 && isxdigit(str[pos]); p++) {
                        w += str[pos++];
                     }
                     word += (char)strtoul(w.c_str(), NULL, 16);
                     --pos;
                  } else word += 'x';
                  break;
               default:
                  if (str[pos] >= '0' && str[pos] <= '7') {
                     /* Process octal escape sequence */
                     w.clear();
                     for (p = 0; pos < slen && p < 3 && str[pos] >= '0' && str[pos] <= '7'; p++) {
                        w += str[pos++];
                     }
                     word += (char)strtoul(w.c_str(), NULL, 8);
                     --pos;
                  } else {
                     /* anything else following an escape is a literal */
                     word += str[pos];
                  }
               }
               ++pos;
            } else {
               word += w;
            }
         }
         if (c != '"') {
            /* Closing '"' not found */
            return -1;
         }
      } else if (c == ' ' || c == '\n') {
         /* Extracting  unquoted whitespace. Replace with single SP */
         if (c == ' ') word += ' ';
      }
      else {
         /* Extracting atom or special char */
         word += w;
      }
      c = tParseStandard(w, str, pos, "\",");
   }
   /* Found a field */
   if (c == '\n') {
      /* If line ending terminated field, put it back */
      pos -= w.size();
   }
   return 1;
}
