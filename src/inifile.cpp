/* inifile.cpp
 *
 *  Copyright (C) 2013-2014 Josh Fisher
 *
 *  This program is free software. You may redistribute it and/or modify
 *  it under the terms of the GNU General Public License, as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  See the file "COPYING".  If not,
 *  see <http://www.gnu.org/licenses/>.
*/

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_CTYPE_H
#include <ctype.h>
#endif

#include "inifile.h"
#include "loghandler.h"

static tString EMPTY_VAL;

////////////////////////////////////////////////////////////////////////////////
//   Class IniValue
////////////////////////////////////////////////////////////////////////////////

IniValue& IniValue::operator=(const IniValue &b)
{
   if (&b != this) {
      type = b.type;
      value = b.value;
   }
   return *this;
}

IniValue& IniValue::operator=(const tStringArray &b)
{
   value.clear();
   if (b.empty()) return *this;
   if (type == INIKEYWORDTYPE_MULTISZ || type == INIKEYWORDTYPE_SECTION
         || type == INIKEYWORDTYPE_ORDERED_SECTION) {
      value = b;
      return *this;
   }
   /* For types that allow only one string, use only the first string of b */
   value.push_back(b[0]);
   return *this;
}

IniValue& IniValue::operator=(const tString &b)
{
   value.clear();
   value.push_back(b); /* If b is NULL, then value is cleared */
   return *this;
}

IniValue& IniValue::operator=(const char *b)
{
   value.clear();
   if (b) value.push_back(b); /* If b is NULL, then value is cleared */
   return *this;
}

IniValue& IniValue::operator=(long long b)
{
   char buf[64];
   snprintf(buf, sizeof(buf), "%lld", b);
   value.clear();
   value.push_back(buf);
   return *this;
}

IniValue& IniValue::operator=(unsigned long long b)
{
   char buf[64];
   snprintf(buf, sizeof(buf), "%llu", b);
   value.clear();
   value.push_back(buf);
   return *this;
}

IniValue& IniValue::operator=(long b)
{
   char buf[64];
   snprintf(buf, sizeof(buf), "%ld", b);
   value.clear();
   value.push_back(buf);
   return *this;
}

IniValue& IniValue::operator=(unsigned long b)
{
   char buf[64];
   snprintf(buf, sizeof(buf), "%lu", b);
   value.clear();
   value.push_back(buf);
   return *this;
}


IniValue& IniValue::operator=(int b)
{
   char buf[64];
   snprintf(buf, sizeof(buf), "%d", b);
   value.clear();
   value.push_back(buf);
   return *this;
}

IniValue& IniValue::operator=(unsigned int b)
{
   char buf[64];
   snprintf(buf, sizeof(buf), "%u", b);
   value.clear();
   value.push_back(buf);
   return *this;
}

IniValue& IniValue::operator=(short b)
{   if (!b) return *this;

   char buf[64];
   snprintf(buf, sizeof(buf), "%hd", b);
   value.clear();
   value.push_back(buf);
   return *this;
}

IniValue& IniValue::operator=(unsigned short b)
{
   char buf[64];
   snprintf(buf, sizeof(buf), "%hu", b);
   value.clear();
   value.push_back(buf);
   return *this;
}

IniValue& IniValue::operator=(char b)
{
   char buf[64];
   snprintf(buf, sizeof(buf), "%hhd", b);
   value.clear();
   value.push_back(buf);
   return *this;
}

IniValue& IniValue::operator=(unsigned char b)
{
   char buf[64];
   snprintf(buf, sizeof(buf), "%hhu", b);
   value.clear();
   value.push_back(buf);
   return *this;
}

IniValue& IniValue::operator=(long double b)
{
   char buf[64];
   snprintf(buf, sizeof(buf), "%.21Lg", b);
   value.clear();
   value.push_back(buf);
   return *this;
}

IniValue& IniValue::operator=(double b)
{
   char buf[64];
   snprintf(buf, sizeof(buf), "%.17g", b);
   value.clear();
   value.push_back(buf);
   return *this;
}

IniValue& IniValue::operator=(float b)
{
   char buf[64];
   snprintf(buf, sizeof(buf), "%.8g", b);
   value.clear();
   value.push_back(buf);
   return *this;
}

IniValue& IniValue::operator=(bool b)
{
   value.clear();
   if (b) value.push_back("1");
   else value.push_back("0");
   return *this;
}

IniValue& IniValue::operator+=(const IniValue &b)
{
   tStringArray::const_iterator p;
   if (!b.IsSet()) return *this;
   if (type == INIKEYWORDTYPE_MULTISZ || type == INIKEYWORDTYPE_SECTION
         || type == INIKEYWORDTYPE_ORDERED_SECTION) {
      for (p = b.value.begin(); p != b.value.end(); p++)
         value.push_back(*p);
   } else {
      if (value.empty()) value.push_back("");
      value[0] += b.value[0];
   }
   return *this;
}

IniValue& IniValue::operator+=(const tStringArray &b)
{
   tStringArray::const_iterator p;
   if (b.empty()) return *this;
   if (type == INIKEYWORDTYPE_MULTISZ || type == INIKEYWORDTYPE_SECTION
         || type == INIKEYWORDTYPE_ORDERED_SECTION) {
      for (p = b.begin(); p != b.end(); p++)
         value.push_back(*p);
   } else {
      if (value.empty()) value.push_back("");
      value[0] += b[0];
   }
   return *this;
}

IniValue& IniValue::operator+=(const tString &b)
{
   if (value.empty() || type == INIKEYWORDTYPE_MULTISZ || type == INIKEYWORDTYPE_SECTION
         || type == INIKEYWORDTYPE_ORDERED_SECTION) {
      value.push_back(b);
   } else {
      if (value.empty()) value.push_back("");
      value[0] += b;
   }
   return *this;
}

IniValue& IniValue::operator+=(const char *b)
{
   if (!b) return *this;
   if (value.empty() || type == INIKEYWORDTYPE_MULTISZ || type == INIKEYWORDTYPE_SECTION
         || type == INIKEYWORDTYPE_ORDERED_SECTION) {
      value.push_back(b);
   } else {
      if (value.empty()) value.push_back("");
      value[0] += b;
   }
   return *this;
}

IniValue& IniValue::operator+=(const char c)
{
   char buf[2];
   buf[0] = c;
   buf[1] = 0;
   *this += buf;
   return *this;
}

size_t IniValue::size() const
{
   if (type == INIKEYWORDTYPE_MULTISZ || type == INIKEYWORDTYPE_SECTION
         || type == INIKEYWORDTYPE_ORDERED_SECTION) {
      return value.size();
   }
   if (value.empty()) return 0;
   return value[0].size();
}

const char* IniValue::GetSZ() const
{
   if (value.empty()) return "";
   return value[0].c_str();
}

const tString& IniValue::GetString() const
{
   if (value.empty()) return EMPTY_VAL;
   return value.front();
}

long long IniValue::GetLONG() const
{
   if (value.empty()) return 0;
   return strtoll(value[0].c_str(), NULL, 10);
}

unsigned long long IniValue::GetULONG() const
{
   if (value.empty()) return 0;
   return strtoull(value[0].c_str(), NULL, 10);
}

long double IniValue::GetDOUBLE() const
{
   if (value.empty()) return 0;
   return strtold(value[0].c_str(), NULL);
}

bool IniValue::GetBOOL() const
{
   if (value.empty()) return false;
   tString v(value[0]);
   tToLower(v);
   long l = strtol(v.c_str(), NULL, 10);
   if (v == "yes" || v == "y" || v == "true" || v == "t"
         || v == "on" || (isdigit(v[0]) && l != 0)) return true;
   return false;
}

////////////////////////////////////////////////////////////////////////////////
//   Class IniFile
////////////////////////////////////////////////////////////////////////////////

IniFile::IniFile(const IniFile &b)
{
   kwmap = b.kwmap;
}

/*
 *  Assignment operator
 */
IniFile& IniFile::operator=(const IniFile &b)
{
   if (&b != this) {
      kwmap = b.kwmap;
    }
   return *this;
}


/*
 * Method to parse key and return section, keyword, and if ordered section, then also ordered section
 * base name and the index of the keyword in the section's map entry
 */
bool IniFile::ParseKey(const char *key_in, tString &base, size_t &ndx, tString &sect, tString &kw)
{
   size_t n;
   tString sndx, key(key_in);
   base.clear();
   sect.clear();
   kw.clear();
   ndx = 0;
   tToLower(tRemoveWS(key));
   if (key.find_first_of("=%,;") != tString::npos) return false;
   n = key.find('/');
   if (n == 0) {
      key.erase(0, 1);
      n = key.find('/');
      if (n == 0) return false;
   }
   if (key.empty()) return false;
   if (n == tString::npos) {
      /* Key in global section */
      kw = key;
      return true;
   }
   sect = key.substr(0, n);
   kw = key.substr(n + 1);
   if (kw.empty()) {
      sect.clear();
      return false;
   }
   n = sect.find_last_not_of("0123456789");
   if (n == tString::npos) {
      /* numeric section names are invalid */
      sect.clear();
      kw.clear();
      return false;
   }
   ++n;
   if (n < sect.size()) {
      sndx = sect.substr(n);
      ndx = strtoul(sndx.c_str(), NULL, 10);
      base = sect.substr(0, n);
   }
   return true;
}

IniValuePairList::iterator IniFile::FindKey(const char *key_in)
{
   IniValuePairList::iterator p;
   tString key(key_in);
   tToLower(tRemoveWS(key));
   for (p = kwmap.begin(); p != kwmap.end(); p++) {
      if (p->key == key) break;
   }
   return p;
}

IniValuePairList::iterator IniFile::FindValue(const char *key)
{
   size_t n, s, ndx;
   IniValuePair np;
   IniValuePairList::iterator p;
   tString sect, kw, base;

   p = FindKey(key);
   if (p != kwmap.end()) {
      if (p->value.type == INIKEYWORDTYPE_SECTION || p->value.type == INIKEYWORDTYPE_ORDERED_SECTION)
         return kwmap.end(); /* a section key has no associated value */
      return p; /* found key's value */
   }
   /* For ordered sections, key values are not created in map until they are accessed or assigned.
    * Therefore we must determine if this is a valid ordered section key */
   if (!ParseKey(key, base, ndx, sect, kw)) {
      /* Invalid key syntax */
      return kwmap.end();
   }
   if (base.empty()) {
      /* Not an ordered section, so key does not exist */
      return kwmap.end();
   }
   p = FindKey(base);
   if (p == kwmap.end()) {
      /* No such ordered section found */
      return kwmap.end();
   }
   if (p->value.type != INIKEYWORDTYPE_ORDERED_SECTION) {
      /* Not a valid ordered section */
      return kwmap.end();
   }
   /* Check that ordered section defines the given keyword */
   for (s = 0; s < p->value.value.size(); s++) {
      n = p->value.value[s].find(',');
      if (p->value.value[s].substr(0, n) == kw) break;
   }
   if (s == p->value.value.size()) {
      /* keyword not found for this ordered section */
      return kwmap.end();
   }
   /* Found valid ordered section key that needs to be created */
   np.key = sect + '/';
   np.key += kw;
   np.value.type = (int)strtol(p->value.value[s].substr(n + 1).c_str(), NULL, 10);
   kwmap.push_back(np);
   p = kwmap.end();
   --p;
   return p;
}

IniValuePairList::iterator IniFile::FindSection(const char *sect_in)
{
   IniValuePairList::iterator p;
   tString sect(sect_in);
   tToLower(tRemoveWS(sect));
   if (sect.empty() || sect.find_first_of("=%,;/") != tString::npos) return kwmap.end();
   p = FindKey(sect);
   if (p != kwmap.end()) {
      if (p->value.type == INIKEYWORDTYPE_SECTION || p->value.type == INIKEYWORDTYPE_ORDERED_SECTION)
         return p; /* found section */
      return kwmap.end(); /* key name is not a section */
   }
   /* Check for ordered section */
   size_t n = sect.find_last_not_of("0123456789");
   if (n == tString::npos) return kwmap.end(); /* invalid section name */
   sect.erase(n + 1);
   p = FindKey(sect);
   if (p != kwmap.end() && p->value.type == INIKEYWORDTYPE_ORDERED_SECTION) {
      /* found ordered section */
      return p;
   }
   return kwmap.end(); /* no such section */
}

IniValue& IniFile::operator[](const char *key)
{
   IniValuePairList::iterator p = FindValue(key);
   if (p != kwmap.end()) return p->value; /* found existing keyword */
   return bogus;
}

/*
 * Method to retrieve section, keyword, and ordered section info of an existing key
 * Returns true if key found, else false
 */
bool IniFile::GetKey(const char *key, tString &base, size_t &ndx, tString &sect, tString &kw)
{
   IniValuePairList::iterator p = FindValue(key);
   if (p == kwmap.end()) return false; /* Key not defined */
   return ParseKey(p->key.c_str(), base, ndx, sect, kw);
}

/*
 * Method to add a global keyword to the grammar
 */
bool IniFile::AddKeyword(const char *key, int kwtype)
{
   IniValuePair np;
   IniValuePairList::iterator p = FindValue(key);
   if (p != kwmap.end()) {
      err_msg = "keyword already exists";
      return false;
   }
   np.key = key;
   tToLower(tRemoveWS(np.key));
   if (np.key.find_first_of("/=%,;") != tString::npos) {
      err_msg = "invalid keyword";
      return false;
   }
   if (kwtype < INIKEYWORDTYPE_SZ || kwtype > INIKEYWORDTYPE_BOOL) {
      err_msg = "invalid key type";
      return false;
   }
   np.value.type = kwtype;
   kwmap.push_back(np);
   return true;
}

/*
 * Method to add a section to the grammar
 */
bool IniFile::AddSection(const char *sect, bool is_ordered)
{
   size_t n;
   IniValuePair np;
   IniValuePairList::iterator p = FindValue(sect);

   /* Make sure section name is unused */
   if (p != kwmap.end()) {
      err_msg = "section name conflicts with an existing global keyword";
      return false;
   }
   p = FindKey(sect);
   if (p != kwmap.end()) {
      err_msg = "section name already defined";
      return false;
   }
   /* Validate section name */
   np.key = sect;
   tToLower(tRemoveWS(np.key));
   if (np.key.find_first_of("/=,;") != tString::npos) {
      err_msg = "invalid section name";
      return false; /* invalid params */
   }
   n = np.key.find_last_not_of("0123456789");
   if (n != tString::npos) {
      if (np.key.find_first_not_of("0123456789", n + 1) == tString::npos) {
         /* Section base name with trailing numeric is invalid */
         return false;
      }
   }
   if (is_ordered) {
      np.value.type = INIKEYWORDTYPE_ORDERED_SECTION;
   } else {
      np.value.type = INIKEYWORDTYPE_SECTION;
   }
   kwmap.push_back(np);
   return true;
}

/*
 * Method to add a section specific keyword to the grammar
 */
bool IniFile::AddSectionKeyword(const char *sect_in, const char *kw_in, int kwtype)
{
   size_t n, s, ndx;
   IniValuePairList::iterator p;
   IniValuePair np;
   tString kw, sect, base, buf;
   char cbuf[64];

   if (kwtype < INIKEYWORDTYPE_SZ || kwtype > INIKEYWORDTYPE_BOOL) {
      err_msg = "invalid value type";
      return false;
   }
   p = FindKey(sect_in);
   if (p == kwmap.end()) {
      err_msg = "no such section";
      return false;
   }
   /* Check for section keyword already defined */
   buf = p->key + '/';
   buf += kw_in;
   if (!ParseKey(buf, base, ndx, sect, kw)) {
      err_msg = "invalid keyword";
      return false;
   }
   for (s = 0; s < p->value.value.size(); s++) {
      n = p->value.value[s].find(',');
      if (p->value.value[s].substr(0, n) == kw) break;
   }
   if (s < p->value.value.size()) {
      err_msg = "duplicate keyword defined for this section";
      return false;
   }
   /* Add keyword to this section's list of valid keywords */
   snprintf(cbuf, sizeof(cbuf), ",%d", kwtype);
   buf = kw + cbuf;
   p->value.value.push_back(buf);
   /* For unordered sections, add full keyword to map */
   if (p->value.type == INIKEYWORDTYPE_SECTION) {
      np.key = sect + '/';
      np.key += kw;
      np.value.type = kwtype;
      kwmap.push_back(np);
   }
   return true;
}


/*
 * Method to clear value of all keywords
 */
void IniFile::ClearKeywordValues()
{
   IniValuePairList::iterator p;
   for (p = kwmap.begin(); p != kwmap.end(); p++) {
      if (p->value.type != INIKEYWORDTYPE_SECTION
            && p->value.type != INIKEYWORDTYPE_ORDERED_SECTION) {
         p->value.clear();
      }
   }
}

/*
 * Method to update keyword values with values from keywords
 * in b that have been set.
 */
bool IniFile::Update(const IniFile &b)
{
   IniValuePairList::iterator p;
   IniValuePairList::const_iterator pb;
   for (pb = b.kwmap.begin(); pb != b.kwmap.end(); pb++) {
      /* Skip section entries */
      if (pb->value.type == INIKEYWORDTYPE_SECTION
            || pb->value.type == INIKEYWORDTYPE_ORDERED_SECTION) continue;
      /* Find key in this matching key in b */
      p = FindValue(pb->key.c_str());
      if (p == kwmap.end()) {
         /* Key not found */
         err_msg = "key '" + pb->key + "' is not defined";
         return false;
      }
      /* Found key in this matching key in b */
      if (pb->value.IsSet()) {
         /* Update from values in b that have been set */
         if (p->value.type == INIKEYWORDTYPE_MULTISZ) {
            /* For string list value types, an update appends b's value to this */
            if (pb->value.value.empty() || pb->value.value[0].empty()) {
               /* An empty value acts as a reset, clearing existing values */
               p->value.clear();
            } else {
               p->value += pb->value;
            }
         } else {
            /* For all other types, b value replaces existing value */
            p->value = pb->value;
         }
      }
   }
   return true;
}

/*
 * Method to get list of valid keywords
 */
size_t IniFile::GetKeywords(tStringArray &kw)
{
   size_t n, v;
   tString sect, tmp;
   IniValuePairList::iterator p;
   kw.clear();
   for (p = kwmap.begin(); p != kwmap.end(); p++)
   {
      if (p->value.type == INIKEYWORDTYPE_SECTION) {
         /* add this unordered section's keywords */
         sect = p->key + "/";
         for (v = 0; v < p->value.value.size(); v++) {
            n = p->value.value[v].find(',');
            kw.push_back(sect + p->value.value[v].substr(0, n));
         }
      } else if (p->value.type == INIKEYWORDTYPE_ORDERED_SECTION) {
         /* add this ordered section's keywords */
         sect = p->key + "%lu/";
         for (v = 0; v < p->value.value.size(); v++) {
            n = p->value.value[v].find(',');
            kw.push_back(sect + p->value.value[v].substr(0, n));
         }
      } else {
         /* Add global section keywords */
         if (p->key.find('/') == tString::npos)
            kw.push_back(p->key);
      }
   }
   return kw.size();
}

/*
 * Method to get list of keywords that have been assigned values
 */
size_t IniFile::GetSetKeywords(tStringArray &kw)
{
   IniValuePairList::iterator p;
   kw.clear();
   for (p = kwmap.begin(); p != kwmap.end(); p++)
   {
      if (p->value.type == INIKEYWORDTYPE_SECTION
            || p->value.type == INIKEYWORDTYPE_ORDERED_SECTION) continue;
      if (p->value.IsSet()) {
         kw.push_back(p->key);
      }
   }
   return kw.size();
}


/*
 * Method to parse keyword/value pairs from a file
 * returns:
 *     0   EOF
 *    -1   I/O error
 *    -2   quoted string missing closing quote
 *    -3   invalid escape sequence
 *    ' '  string of blank chars
 *    '\n' newline
 *    '['  [
 *    ']'  ]
 *    '='  =
 *    'S'  qouted string
 *    'A'  atom text
 */
int IniFile::ReadToken(FILE* fs, tString &str)
{
   tString word, tmp, special = "=[]\\\n";
   int c, delim = 0, esc = 0;

   str.clear();
   c = fgetc(fs);
   /* Strip comments */
   if (c == ';' || c == '#') {
      c = fgetc(fs);
      while (c != EOF && c != '\n') {
         c = fgetc(fs);
      }
   }
   /* Check for EOF */
   if (c == EOF) {
      if (ferror(fs)) return -1; /* i/o error */
      return 0; /* normal end of file */
   }
   /* Special handling for CRLF line endings */
   if (c == '\r') {
      c = fgetc(fs);
      if (c != '\n') return -1; /* CR not followed by LF is an error */
   }
   /* Look for single char tokens */
   if (special.find((char)c) != tString::npos) {
      str = (char)c;
      return c;
   }
   /* look for space token */
   if (c == ' ' || c == '\t') {
      while (c == ' ' || c == '\t') {
         str += (char)c;
         c = fgetc(fs);
      }
      if (c != EOF) ungetc(c, fs);
      return ' ';
   }
   /* look for quoted string token */
   if (c == '"' || c == '\'') {
      delim = c;
      c = fgetc(fs);
      while (c != EOF) {
         if (!esc) {
            if (c == delim) return 'S';
            if (c == '\\') esc = 1;
            else str += (char)c;
         } else {
            switch (c)
            {
            case '0':
               str += '\0';
               break;
            case 'r':
               str += '\r';
               break;
            case 'n':
               str += '\n';
               break;
            case 't':
               str += '\t';
               break;
            case 'x':
               tmp.clear();
                c = fgetc(fs);
               while (c != EOF && tmp.size() < 2 && isxdigit(c)) {
                  tmp += tolower(c);
                  c = fgetc(fs);
               }
               if (c != EOF) ungetc(c, fs);
               if (tmp.empty()) return -3;
               str += (char)strtol(tmp.c_str(), NULL, 16);
               break;
            default:
               str += (char)c;
               break;
            }
            esc = 0;
         }
         c = fgetc(fs);
      }
      return -2; /* closing delimiter not found */
   }
   /* look for atom */
   while (c != EOF && c != '"' && c != '\'' && c != ' ' && c != '\t' && c
         != '=' && c != '[' && c != ']' && c != '\r' && c != '\n' && c != '\\') {
      str += (char)c;
      c = fgetc(fs);
   }
   if (c != EOF) ungetc(c, fs);
   return 'A';
}

/*
 * Method to parse keyword/value pairs from a file
 */
int IniFile::Read(const char *fname)
{
   if (!fname || !strlen(fname)) {
      err_msg = "filename is empty or null";
      return -1;
   }
   FILE* fin = fopen(fname, "r");
   if (!fin) {
      err_msg = fname;
      err_msg += " not found";
      return -1;
   }
   int result = Read(fin);
   fclose(fin);
   return result;
}

/*
 * Method to parse keyword/value pairs from an open file
 */
int IniFile::Read(FILE *fin)
{
   int tok, state = 0, linenum = 1;
   tString word, section;
   tString kw, kwtmp, val, tmp;
   tString skw_name, sval;
   tStringArray kwlist;
   IniValue ival;
   IniValuePairList::iterator p = kwmap.end(), ps;
   char buf[4096];

   err_msg.clear();
   if (!fin || ferror(fin)) {
      err_msg = "bad file stream";
      return -1;
   }
   if (feof(fin)) return 0;

   tok = ReadToken(fin, word);
   while (tok > 0) {
      switch (state)
      {
      case 0:
         /* Looking for start of token */
         switch (tok)
         {
         case '\n':
            ++linenum;
            break;
         case ' ':
            break;
         case '[':
            state = 5;
            break;
         case 'A':
            kwtmp = word;
            state = 1;
            break;
         default:
            snprintf(buf, sizeof(buf), "syntax error on line %d", linenum);
            err_msg = buf;
            return linenum;
         }
         break;
      case 1:
         /* Reading keyword */
         switch (tok)
         {
         case ' ':
            break;
         case 'A':
            kwtmp += word;
            break;
         case '=':
            /* Found '=' marking end of keyword */
            if (section.empty()) kw.clear();
            else kw = section + "/";
            kw += kwtmp;
            val.clear();
            /* Check that keyword is defined */
            p = FindValue(kw);
            if (p == kwmap.end()) {
               snprintf(buf, sizeof(buf), "unknown keyword '%s' on line %d", kw.c_str(), linenum);
               err_msg = buf;
               return linenum;
            }
            state = 2;
            break;
         default:
            snprintf(buf, sizeof(buf), "invalid keyword on line %d", linenum);
            err_msg = buf;
            return linenum;
         }
         break;
      case 2:
         /* find start of value */
         switch (tok)
         {
         case ' ':
            break;
         case '\\':
            state = 18;
            break;
         case ',':
            /* comma is first token of value */
            if (p->value.type == INIKEYWORDTYPE_MULTISZ) {
               p->value += "";
               state = 7;
            } else {
               val = word;
               state = 3;
            }
            break;
         case '\n':
            /* No value given means keyword's value should be cleared */
            p->value.clear();
            p = kwmap.end();
            ++linenum;
            state = 0;
            break;
         default:
            /* Found first word of value */
            val = word;
            if (p->value.type == INIKEYWORDTYPE_MULTISZ)
               state = 7;
            else
               state = 3;
            break;
         }
         break;
      case 3:
         /* reading value */
         switch (tok)
         {
         case '\\':
            /* Found line continuation */
            state = 4;
            break;
         case '\n':
            /* End of line marks end of value */
            p->value = val;
            p = kwmap.end();
            ++linenum;
            state = 0;
            break;
         default:
            val += word;
            break;
         }
         break;
      case 4:
         /* looking for continuation line */
         switch (tok)
         {
         case ' ':
            break;
         case '\n':
            /* Found EOL after line continuation symbol '/' */
            ++linenum;
            if (p->value.type == INIKEYWORDTYPE_MULTISZ)
               state = 7;
            else
               state = 3;
            break;
         default:
            snprintf(buf, sizeof(buf), "invalid continuation of line %d", linenum);
            err_msg = buf;
            return linenum;
         }
         break;
      case 5:
         /* Find section name following '[' */
         switch (tok)
         {
         case ' ':
            break;
         case 'A':
            /* Found first word of section name */
            section = word;
            state = 6;
            break;
         case ']':
            /* blank or empty section name means return scope to global section */
            section.clear();
            state = 0;
            break;
         default:
            snprintf(buf, sizeof(buf), "invalid section name on line %d", linenum);
            err_msg = buf;
            return linenum;
         }
         break;
      case 6:
         /* Reading section name */
         switch (tok)
         {
         case ' ':
            break;
         case 'A':
            section += word;
            break;
         case ']':
            /* Found section name */
            tToLower(section);
            if (section == "global") {
               /* return scope to global section */
               section.clear();
               state = 0;
               break;
            }
            if (section.find_first_of("/=,;%") != tString::npos) {
               snprintf(buf, sizeof(buf), "invalid section name on line %d", linenum);
               err_msg = buf;
               return linenum;
            }
            /* Check that section name is defined */
            ps = FindSection(section);
            if (ps == kwmap.end()) {
               /* Section not defined */
               snprintf(buf, sizeof(buf), "invalid section name on line %d", linenum);
               err_msg = buf;
               return linenum;
            }
            state = 0;
            break;
         default:
            snprintf(buf, sizeof(buf), "invalid section name on line %d", linenum);
            err_msg = buf;
            return linenum;
         }
         break;
      case 7:
         /* Reading a multi_sz keyword's value */
         switch (tok)
         {
         case '\\':
            state = 4;
            break;
         case ',':
            p->value += val;
            val.clear();
            state = 8;
            break;
         case '\n':
            p->value += val;
            p = kwmap.end();
            ++linenum;
            val.clear();
            state = 0;
            break;
         default:
            val += word;
            break;
         }
         break;
      case 8:
         switch (tok)
         {
         case '\\':
            state = 9;
            break;
         case ' ':
            break;
         case ',':
            p->value += "";
            break;
         case '\n':
            p->value += "";
            p = kwmap.end();
            ++linenum;
            val.clear();
            state = 0;
            break;
         default:
            val = word;
            state = 7;
            break;
         }
         break;
      case 9:
         /* continuation found after comma while looking for begin of value */
         switch (tok)
         {
         case ' ':
            break;
         case '\n':
            ++linenum;
            state = 8;
            break;
         default:
            snprintf(buf, sizeof(buf), "invalid continuation of line %d", linenum);
            err_msg = buf;
            return linenum;
         }
         break;
      case 18:
         /* continuation found while looking for begin of value */
         switch (tok)
         {
         case ' ':
            break;
         case '\n':
            ++linenum;
            state = 2;
            break;
         default:
            snprintf(buf, sizeof(buf), "invalid continuation of line %d", linenum);
            err_msg = buf;
            return linenum;
         }
         break;
      }
      tok = ReadToken(fin, word);
   }

   /* Check state at EOF for error */
   if (tok < 0) {
      err_msg = "file i/o error";
      return -1;
   }
   switch (state)
   {
   case 2:
      // found keyword, but EOF before value
      p->value.clear();
      break;
   case 3:
      // EOF before LF reading value
      p->value = val;
      break;
   case 7:
      // EOF before LF reading MULTISZ value
      p->value += val;
      break;
   case 8:
      // EOF after comma reading MULTISZ value
      p->value += "";
      break;
   case 1:
      // EOF before '=' reading keyword
      snprintf(buf, sizeof(buf), "invalid keyword on line %d", linenum);
      err_msg = buf;
      return linenum;
   case 4:
   case 9:
   case 18:
      // EOF follows a line continuation
      snprintf(buf, sizeof(buf), "invalid continuation on line %d", linenum);
      err_msg = buf;
      return linenum;
   case 5:
   case 6:
      // EOF before closing ']' reading section name
      snprintf(buf, sizeof(buf), "invalid section name on line %d", linenum);
      err_msg = buf;
      return linenum;
   }

   return 0;
}
