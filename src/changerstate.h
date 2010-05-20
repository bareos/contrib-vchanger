/* changerstate.h
 *
 *  This file is part of vchanger by Josh Fisher.
 *
 *  vchanger copyright (C) 2008-2010 Josh Fisher
 *
 *  vchanger is free software.
 *  You may redistribute it and/or modify it under the terms of the
 *  GNU General Public License version 2, as published by the Free
 *  Software Foundation.
 *
 *  vchanger is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with vchanger.  See the file "COPYING".  If not,
 *  write to:  The Free Software Foundation, Inc.,
 *             59 Temple Place - Suite 330,
 *             Boston,  MA  02111-1307, USA.
*/
#ifndef CHANGERSTATE_H_
#define CHANGERSTATE_H_

class VolumeLabel
{
public:
	VolumeLabel();
	char* GetLabel(char *buf, size_t buf_sz);
	int set(const char *chgr, int magnum, int magslot);
	int set(const char *vlabel);
	void clear();
	bool empty() { return mag_number < 1 ? true : false; }
   bool operator==(const VolumeLabel &b);
   bool operator!=(const VolumeLabel &b);
   VolumeLabel& operator=(const VolumeLabel &b);
public:
	int mag_number;
	int mag_slot;
	char chgr_name[128];
};

class DriveState
{
public:
	DriveState();
	int set(const VolumeLabel &vlabel);
	int set(const VolumeLabel *vlabel);
	int set(const char *vname);
	int clear();
	int restore();
	bool empty() { return vol.empty(); }
protected:
   int save();
public:
	VolumeLabel vol;
	int drive;
};

class MagazineState
{
public:
	MagazineState();
	virtual ~MagazineState();
	int set(const char *magname, const char *automount_dir = NULL);
	void clear();
	int restore();
protected:
	int ReadMagazineIndex();
	int save();
public:
   int bay_ndx;
	int mag_number;
	char changer[256];
	char mag_name[256];
	char mountpoint[PATH_MAX];
};

#endif /*CHANGERSTATE_H_*/
