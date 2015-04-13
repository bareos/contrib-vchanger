/*  tstring.h
 *
 *  This file is part of msafe by Josh Fisher.
 *
 *  vchanger copyright (C) 2006-2014 Josh Fisher
 *
 *  vchanger is free software.
 *  You may redistribute it and/or modify it under the terms of the
 *  GNU General Public License, as published by the Free Software
 *  Foundation; either version 2, or (at your option) any later version.
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
#ifndef _TSTRING_H_
#define _TSTRING_H_

#include <string>
#include <list>
#include <vector>
#include <map>

typedef std::basic_string<char> tString;
typedef tString::iterator tStringIterator;
typedef tString::const_iterator tStringConstIterator;
typedef tString::reverse_iterator tStringReverseIterator;
typedef tString::const_reverse_iterator tStringConstReverseIterator;
typedef std::list<tString> tStringList;
typedef tStringList::iterator tStringListIterator;
typedef tStringList::const_iterator tStringListConstIterator;
typedef tStringList::reverse_iterator tStringListReverseIterator;
typedef tStringList::const_reverse_iterator tStringListConstReverseIterator;
typedef std::vector<tString> tStringArray;
typedef std::map<tString, tString> tStringMap;
typedef std::map<tString, tStringList> tStringListMap;

tString& tToLower(tString &b);
tString tToLower(const char *sin);
tString& tToUpper(tString &b);
tString tToUpper(const char *sin);
tString& tStripLeft(tString &b);
tString tStripLeft(const char *sin);
tString& tStripRight(tString &b);
tString tStripRight(const char *sin);
tString& tStrip(tString &b);
tString tStrip(const char *sin);
tString& tRemoveWS(tString &b);
tString tRemoveWS(const char *b);
tString& tRemoveEOL(tString &b);
tString tRemoveEOL(const char *sin);
tString& tLF2CRLF(tString &b);
tString tLF2CRLF(const char *sin);
tString& tCRLF2LF(tString &b);
tString tCRLF2LF(const char *sin);
tString tExtractLine(const tString &b, size_t &pos);
tString tExtractLine(const char *sin, size_t &pos);
size_t tCaseFind(const tString &str, char c, size_t pos = 0);
size_t tCaseFind(const tString &str, const char *pat, size_t pos = 0);
inline size_t tCaseFind(const tString &str, const tString &pat, size_t pos = 0)
   { return tCaseFind(str, pat.c_str(), pos); }
int tCaseCmp(const char *a, const char *b);
inline int tCaseCmp(const tString &a, const char *b) { return tCaseCmp(a.c_str(), b); }
inline int tCaseCmp(const char *a, const tString &b) { return tCaseCmp(a, b.c_str()); }
inline int tCaseCmp(const tString &a, const tString &b) { return tCaseCmp(a.c_str(), b.c_str()); }
const char* tGetLine(tString &str, FILE *FS);
const char* tFormat(tString &str, const char *fmt, ...);
char tParseStandard(tString &word, const char *str, size_t &pos, tString special = "", tString ws = "");
int tParseCSV(tString &word, const char *str, size_t &pos);
inline int tParseCSV(tString &word, const tString &str, size_t &pos)
   { return tParseCSV(word, str.c_str(), pos); }

#endif
