/* changerstate.h
 *
 *  This file is part of vchanger by Josh Fisher.
 *
 *  vchanger copyright (C) 2008-2009 Josh Fisher
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

class DriveState
{
public:
	DriveState();
	virtual ~DriveState();
	int set(const char *chgrname, int magnum, int magslot);
	int set(const char *volname);
	void unset();
	int save();
	int restore();
	void clear();
public:
	int drive;
	int mag_number;
	int mag_slot;
	char statedir[PATH_MAX];
	char chgr_name[128];
};

class MagazineState
{
public:
	MagazineState();
	virtual ~MagazineState();
public:
	int mag_number;
	char mountpoint[PATH_MAX];
};

#endif /*CHANGERSTATE_H_*/
