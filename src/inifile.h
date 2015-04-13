/*  inifile.h
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

#ifndef _INIPARSE_H_
#define _INIPARSE_H_ 1

#include <list>
#include "tstring.h"

#define INIKEYWORDTYPE_SECTION      0
#define INIKEYWORDTYPE_ORDERED_SECTION     1
#define INIKEYWORDTYPE_SZ		      2
#define INIKEYWORDTYPE_MULTISZ      3
#define INIKEYWORDTYPE_LONGLONG     4
#define INIKEYWORDTYPE_ULONGLONG    5
#define INIKEYWORDTYPE_LONG         6
#define INIKEYWORDTYPE_ULONG        7
#define INIKEYWORDTYPE_INT          8
#define INIKEYWORDTYPE_UINT         9
#define INIKEYWORDTYPE_SHORT        10
#define INIKEYWORDTYPE_USHORT       11
#define INIKEYWORDTYPE_CHAR         12
#define INIKEYWORDTYPE_UCHAR        13
#define INIKEYWORDTYPE_LONGDOUBLE   14
#define INIKEYWORDTYPE_DOUBLE       15
#define INIKEYWORDTYPE_FLOAT		   16
#define INIKEYWORDTYPE_BOOL		   17

class IniValue
{
public:
   IniValue() : type(INIKEYWORDTYPE_SZ) {}
   IniValue(const IniValue &b) : type(b.type), value(b.value) {}
   virtual ~IniValue() {}
   IniValue& operator=(const IniValue &b);
   IniValue& operator=(const tStringArray &b);
   IniValue& operator=(const tString &b);
   IniValue& operator=(const char *b);
   IniValue& operator=(long long b);
   IniValue& operator=(unsigned long long b);
   IniValue& operator=(long b);
   IniValue& operator=(unsigned long b);
   IniValue& operator=(int b);
   IniValue& operator=(unsigned int b);
   IniValue& operator=(short b);
   IniValue& operator=(unsigned short b);
   IniValue& operator=(char b);
   IniValue& operator=(unsigned char b);
   IniValue& operator=(long double b);
   IniValue& operator=(double b);
   IniValue& operator=(float b);
   IniValue& operator=(bool b);
   IniValue& operator+=(const IniValue &b);
   IniValue& operator+=(const tStringArray &b);
   IniValue& operator+=(const tString &b);
   IniValue& operator+=(const char *b);
   IniValue& operator+=(const char c);
   long long GetLONG() const;
   unsigned long long GetULONG() const;
   long double GetDOUBLE() const;
   bool GetBOOL() const;
   const char* GetSZ() const;
   const tString& GetString() const;
   size_t size() const;
   inline void SetType(int newtype) { type = newtype; }
   inline void clear() { value.clear(); }
   inline int GetType() const { return type; }
   inline bool empty() const { return size() == 0; }
   inline bool IsSet() const { return value.size() != 0; }
   inline operator long long() { return GetLONG(); }
   inline operator unsigned long long() { return GetULONG(); }
   inline operator long() { return (long)GetLONG(); }
   inline operator unsigned long() { return (unsigned long)GetULONG(); }
   inline operator int() { return (int)GetULONG(); }
   inline operator unsigned int() { return (unsigned int)GetULONG(); }
   inline operator short() { return (short)GetULONG(); }
   inline operator unsigned short() { return (unsigned short)GetULONG(); }
   inline operator char() { return (char)GetULONG(); }
   inline operator unsigned char() { return (unsigned char)GetULONG(); }
   inline operator long double() { return GetDOUBLE(); }
   inline operator double() { return (double)GetDOUBLE(); }
   inline operator float() { return (float)GetDOUBLE(); }
   inline operator bool() { return GetBOOL(); }
   inline operator const char*() const { return GetSZ(); }
   inline operator const tString&() const { return GetString(); }
   inline operator const tStringArray&() const { return value; }
   friend class IniFile;
protected:
   int type;
   tStringArray value;
};


class IniValuePair
{
public:
   IniValuePair() {}
   virtual ~IniValuePair() {}
   inline IniValuePair& operator=(const IniValuePair &b)
   {
      if (&b != this) {
         key = b.key;
         value = b.value;
      }
      return *this;
   }
public:
   tString key;
   IniValue value;
};

typedef std::list<IniValuePair> IniValuePairList;

class IniFile
{
public:
   IniFile() {}
   IniFile(const IniFile &b);
   virtual ~IniFile() {}
   IniFile& operator=(const IniFile &b);
   IniValue& operator[](const char *key);
   inline IniValue& operator[](const tString &key) { return operator[](key.c_str()); }
   inline const IniValue& operator[](const char *key) const
      { return (const IniValue&)((*this)[key]); }
   inline const IniValue& operator[](const tString &key) const
      { return (const IniValue&)((*this)[key.c_str()]); }
   bool AddKeyword(const char *kw, int kwtype = INIKEYWORDTYPE_SZ);
   inline bool AddKeyword(const tString &kw, int kwtype = INIKEYWORDTYPE_SZ)
      { return AddKeyword(kw.c_str(), kwtype); }
   bool AddSection(const char *sect, bool is_ordered = false);
   inline bool AddSection(const tString &sect, bool is_ordered = false)
      { return AddSection(sect.c_str(), is_ordered); }
   bool AddSectionKeyword(const char *sect, const char *kw, int kwtype = INIKEYWORDTYPE_SZ);
   inline bool AddSectionKeyword(const tString &sect, const char *kw, int kwtype = INIKEYWORDTYPE_SZ)
      { return AddSectionKeyword(sect.c_str(), kw, kwtype); }
   inline bool AddSectionKeyword(const char *sect, const tString &kw, int kwtype = INIKEYWORDTYPE_SZ)
      { return AddSectionKeyword(sect, kw.c_str(), kwtype); }
   inline bool AddSectionKeyword(const tString &sect, const tString &kw, int kwtype = INIKEYWORDTYPE_SZ)
      { return AddSectionKeyword(sect.c_str(), kw.c_str(), kwtype); }
   inline bool KeywordExists(const char *kw) { return FindValue(kw) != kwmap.end(); }
   inline bool KeywordExists(const tString &kw) { return FindValue(kw.c_str()) != kwmap.end(); }
   inline bool SectionExists(const char *sect) { return FindSection(sect) != kwmap.end(); }
   inline bool SectionExists(const tString &sect) { return FindSection(sect.c_str()) != kwmap.end(); }
   bool GetKey(const char *key, tString &base, size_t &ndx, tString &sect, tString &kw);
   bool GetKey(const tString &key, tString &base, size_t &ndx, tString &sect, tString &kw)
      { return GetKey(key.c_str(), base, ndx, sect, kw); }
   void ClearKeywordValues();
   bool Update(const IniFile &b);
   int Read(FILE* stream_in);
   int Read(const char *fname);
   inline int Read(const tString &fname) { return Read(fname.c_str()); }
   size_t GetSetKeywords(tStringArray &kw);
   size_t GetKeywords(tStringArray &kw);
   inline void clear() { kwmap.clear(); }
   inline bool empty() { return kwmap.empty(); }
   inline tString GetErrorMessage() { return err_msg; }
protected:
   bool ParseKey(const char *key, tString &base, size_t &ndx, tString &sect, tString &kw);
   inline bool ParseKey(const tString &key, tString &base, size_t &ndx, tString &sect, tString &kw)
      { return ParseKey(key.c_str(), base, ndx, sect, kw); }
   IniValuePairList::iterator FindKey(const char *key);
   inline IniValuePairList::iterator FindKey(const tString &key) { return FindKey(key.c_str()); }
   IniValuePairList::iterator FindValue(const char *key);
   inline IniValuePairList::iterator FindValue(const tString &key) { return FindValue(key.c_str()); }
   IniValuePairList::iterator FindSection(const char *sect);
   inline IniValuePairList::iterator FindSection(const tString &sect) { return FindSection(sect.c_str()); }
   int ReadToken(FILE* fs, tString &str);
protected:
   IniValuePairList kwmap;
   IniValue bogus;
   tString err_msg;
};

#endif /* _INIPARSE_H_ */
