/* vchanger.cpp
 *
 *  This file is part of the vchanger package
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

#include "vchanger.h"
#include <locale.h>
#include <getopt.h>
#include "util.h"
#include "vchanger_common.h"
#include "diskchanger.h"

/*-------------------------------------------------
 *  Commands
 * ------------------------------------------------*/
#define NUM_AUTOCHANGER_COMMANDS 7
static char autochanger_command[NUM_AUTOCHANGER_COMMANDS][32] = { "LIST",
      "SLOTS", "LOAD", "UNLOAD", "LOADED", "INITMAG", "LISTMAGS" };

/*-------------------------------------------------
 *  Command line parameters struct
 * ------------------------------------------------*/
typedef struct _cmdparams_s
{
   bool print_version;
   bool print_help;
   int32_t command;
   int32_t slot;
   int32_t drive;
   int32_t init_mag;
   int32_t init_mag_num;
   char runas_user[128];
   char runas_group[128];
   char config_file[PATH_MAX];
   char archive_device[PATH_MAX];
} CMDPARAMS;
CMDPARAMS cmdl;

/*-------------------------------------------------
 *  Function to print version info to stdout
 *------------------------------------------------*/
static void print_version(void)
{
   print_stdout("%s version %s\n", PACKAGE_NAME, PACKAGE_VERSION);
   print_stdout("\n%s.\n", "Copyright (c) 2008-2009 Josh Fisher");
}

/*-------------------------------------------------
 *  Function to print command help to stdout
 *------------------------------------------------*/
static void print_help(void)
{
   print_stdout("USAGE:\n\n"
      "  %s config_file [options] command slot device drive\n"
      "    Perform Bacula Autochanger API command for virtual\n"
      "    changer defined by vchanger configuration file\n"
      "    'config_file' using 'slot', 'device', and 'drive'\n"
      "  %s config_file [options] INITMAG mag_bay [-m|--magnum mag_number]\n"
      "    Initialize new magazine in magazine bay 'mag_bay'. The list of\n"
      "    assigned magazines is defined by one or more 'magazine'\n"
      "    directives in the specified 'config_file'. The first mounted magazine\n"
      "    from the list of assigned magazines is defined to be in bay 1, the\n"
      "    second mounted magazine in bay 2, etc. If 'mag_num' is not given,\n"
      "    then the next available magazine number is chosen automatically.\n"
      "  %s config_file [options] LISTMAGS\n"
      "    List all magazine bays and their current magazine mountpoints.\n"
      "  %s --version\n"
      "    print version\n"
      "  %s --help\n"
      "    print help\n"
      "\nOptions:\n"
      "    -u, --user=uid       user to run as (when invoked by root)\n"
      "    -g, --group=gid      group to run as (when invoked by root)\n"
      "\nReport bugs to %s.\n", PACKAGE_NAME, PACKAGE_NAME, PACKAGE_NAME,
         PACKAGE_NAME, PACKAGE_NAME, PACKAGE_BUGREPORT);
}

/*-------------------------------------------------
 *  Function to parse command line parameters
 *------------------------------------------------*/
#define LONGONLYOPT_VERSION		0
#define LONGONLYOPT_HELP		1

static int parse_cmdline(int argc, char *argv[])
{
   int c, ndx = 0;
   struct option options[] = {
         { "version", no_argument, 0, LONGONLYOPT_VERSION }, { "help",
               no_argument, 0, LONGONLYOPT_HELP }, { "user", required_argument,
               0, 'u' }, { "group", required_argument, 0, 'g' }, { "magnum",
               required_argument, 0, 'm' }, { 0, 0, 0, 0 } };

   cmdl.print_version = false;
   cmdl.print_help = false;
   cmdl.command = 0;
   cmdl.slot = 0;
   cmdl.drive = 0;
   cmdl.init_mag = 0;
   cmdl.init_mag_num = 0;
   cmdl.runas_user[0] = 0;
   cmdl.runas_group[0] = 0;
   cmdl.config_file[0] = 0;
   cmdl.archive_device[0] = 0;
   /* process the command line */
   for (;;) {
      c = getopt_long(argc, argv, "u:g:m:", options, NULL);
      if (c == -1)
         break;
      ++ndx;
      switch (c) {
      case LONGONLYOPT_VERSION:
         cmdl.print_version = true;
         cmdl.print_help = false;
         return 0;

      case LONGONLYOPT_HELP:
         cmdl.print_version = false;
         cmdl.print_help = true;
         return 0;
         char* BuildVolumeName(char *vname, size_t vname_sz, const char *chngr,
               int magnum, int magslot);
         int ParseVolumeName(const char *vname, char *chgr_name,
               size_t chgr_name_sz, int &mag_num, int &mag_slot);

      case 'm':
         cmdl.init_mag_num = (int32_t)strtol(optarg, NULL, 10);
         if (cmdl.init_mag_num == 0) {
            print_stderr("flag -m must specify a positive integer value\n");
            return -1;
         }
         break;
      case 'u':
         strncpy(cmdl.runas_user, optarg, sizeof(cmdl.runas_user));
         break;
      case 'g':
         strncpy(cmdl.runas_group, optarg, sizeof(cmdl.runas_group));
         break;
      default:
         print_stderr("unknown option %s\n", optarg);
         return -1;
      }
   }

   /* process positional params */
   ndx = optind;
   /* get config_file */
   if (ndx >= argc) {
      print_stderr("missing parameter 1 (config_file)\n");
      return -1;
   }
   strncpy(cmdl.config_file, argv[ndx], sizeof(cmdl.config_file));
   if (!strlen(cmdl.config_file)) {
      print_stderr("missing parameter 1 (config_file)\n");
      return -1;
   }
   ++ndx;
   /* get command */
   if (ndx >= argc) {
      print_stderr("missing parameter 2 (command)\n");
      return -1;
   }
   for (cmdl.command = 0; cmdl.command < NUM_AUTOCHANGER_COMMANDS; cmdl.command++) {
      if (strlen(argv[ndx]) == strlen(autochanger_command[cmdl.command])
            && strcasecmp(argv[ndx], autochanger_command[cmdl.command]) == 0)
         break;
   }
   if (cmdl.command >= NUM_AUTOCHANGER_COMMANDS) {
      print_stderr("%s not a recognized command\n", argv[ndx]);
      print_stderr("%s is config file\n", argv[ndx - 1]);
      return -1;
   }
   ++ndx;
   /* Make sure only INITMAG command has -m flag */
   if (cmdl.init_mag_num && cmdl.command != 5) {
      print_stderr("flag -m not valid for this command\n");
      return -1;
   }
   /* Check param 3 exists */
   if (ndx >= argc) {
      if (cmdl.command < 2 || cmdl.command == 6) {
         return 0; /* LIST, SLOTS, and LISTMAGS commands only have 2 params */
      } else {
         if (cmdl.command == 5) {
            print_stderr("missing parameter 3 (magazine bay)\n");
         } else {
            print_stderr("missing parameter 3 (slot number)\n");
         }
      }
      return -1;
   }
   /* Check for INITMAG command */
   if (cmdl.command == 5) {
      cmdl.init_mag = (int32_t)strtol(argv[ndx], NULL, 10);
      if (cmdl.init_mag < 1) {
         print_stderr("magazine bay must be a positive integer argument\n");
         return -1;
      }
   } else {
      /* Get slot number */char* BuildVolumeName(char *vname, size_t vname_sz,
            const char *chngr, int magnum, int magslot);
      int ParseVolumeName(const char *vname, char *chgr_name,
            size_t chgr_name_sz, int &mag_num, int &mag_slot);

      cmdl.slot = (int32_t)strtol(argv[ndx], NULL, 10);
      ++ndx;
      /* Get archive device */
      if (ndx >= argc) {
         if (cmdl.command < 2)
            return 0;
         print_stderr("missing parameter 4 (archive device)\n");
         return -1;
      }
      strncpy(cmdl.archive_device, argv[ndx], sizeof(cmdl.archive_device));
      ++ndx;
      /* Get drive index */
      if (ndx >= argc) {
         if (cmdl.command < 2)
            return 0;
         print_stderr("missing parameter 5 (drive)\n");
         return -1;
      }
      cmdl.drive = (int32_t)strtol(argv[ndx], NULL, 10);
   }
   ++ndx;
   if (ndx < argc) {
      print_stderr("extraneous parameters\n");
      return -1;
   }

   return 0;
}

/*-------------------------------------------------
 *   List Command
 * Prints a line on stdout for each of the autochanger's slots of the
 * form:
 *       s:barcode
 * where 's' is the one-based slot number and 'barcode' is the "barcode"
 * label of the volume in the slot. The volume in the slot is a file in
 * the magazine directory, and the barcode is the filename of that file.
 * If the filesystem containing the magazine directory is not mounted, then
 * the barcode will be blank for slots in that magazine.
 *------------------------------------------------*/
static int do_list_cmd()
{
   VolumeLabel lab;
   int32_t n, mbay, slot_offset;
   char vlabel[256];

   /* List magazines in the order specified in the config file */
   for (mbay = 1; mbay <= changer.magazine_bays; mbay++) {
      /* Slots 1 through slots_per_mag are on the first magazine listed
       * in the config file, slots slots_per_mag+1 through 2*slots_per_mag
       * are on magazine 2, etc.   */
      slot_offset = (mbay - 1) * changer.slots_per_mag;
      if (changer.magazine[mbay].mag_number > 0) {
         /* list slot and barcode label for mounted magazines */
         for (n = 1; n <= changer.slots_per_mag; n++) {
            lab.set(changer.changer_name, changer.magazine[mbay].mag_number, n);
            print_stdout("%d:%s\n", slot_offset + n, lab.GetLabel(vlabel, sizeof(vlabel)));
         }
      } else {
         /* list slot only for bays with no mounted magazine */
         for (n = 1; n <= changer.slots_per_mag; n++) {
            print_stdout("%d:\n", slot_offset + n);
         }
      }
   }
   return 0;
}

/*-------------------------------------------------
 *   Slots Command
 *------------------------------------------------*/
static int do_slots_cmd()
{
   print_stdout("%d\n", changer.slots);
   return 0;
}

/*-------------------------------------------------
 *   Load Command
 *------------------------------------------------*/
static int do_load_cmd()
{
   char errmsg[4096];
   if (changer.LoadDrive(cmdl.drive, cmdl.slot)) {
      changer.GetLastError(errmsg, sizeof(errmsg));
      vlog.Error("%s: Error! %s\n", changer.changer_name, errmsg);
      print_stderr("%s\n", errmsg);
      return -1;
   }
   return 0;
}

/*-------------------------------------------------
 *   Unload Command
 *------------------------------------------------*/
static int do_unload_cmd()
{
   char errmsg[4096];
   if (changer.UnloadDrive(cmdl.drive)) {
      changer.GetLastError(errmsg, sizeof(errmsg));
      vlog.Error("%s: Error! %s\n", changer.changer_name, errmsg);
      print_stderr("%s\n", errmsg);
      return -1;
   }
   return 0;
}

/*-------------------------------------------------
 *   Loaded Command
 *------------------------------------------------*/
static int do_loaded_cmd()
{
   int32_t slot = changer.GetDriveSlot(cmdl.drive);
   if (slot < 0)
      slot = 0;
   print_stdout("%d\n", slot);
   return 0;
}

/*-------------------------------------------------
 *   INITMAG - Initialize Magazine Command
 *------------------------------------------------*/
static int do_magazine_init(int mag, int magnum)
{
   char errmsg[4096];
   if (magnum < 1)
      magnum = -1;
   int mag_num = changer.CreateMagazine(mag, magnum);
   if (mag_num < 1) {
      changer.GetLastError(errmsg, sizeof(errmsg));
      print_stderr("%s\n", errmsg);
      return -1;
   }
   print_stdout("created magazine %d in bay %d [%s]\n",
         changer.magazine[mag].mag_number, mag,
         changer.magazine[mag].mountpoint);
   return 0;
}

/*-------------------------------------------------
 *   LISTMAGS - List Magazines Command
 *------------------------------------------------*/
static int do_list_magazines()
{
   int n;
   char errmsg[4096];

   for (n = 1; n <= changer.magazine_bays; n++) {
      print_stdout("%d:%s:%d:%s\n", n, changer.magazine[n].changer, changer.magazine[n].mag_number,
            changer.magazine[n].mountpoint);
   }
   return 0;
}

/* -------------  Main  -------------------------*/

int main(int argc, char *argv[])
{
   int32_t error_code;
   char errmsg[4096];

   setlocale(LC_ALL, "");
   /* parse the command line */
   if ((error_code = parse_cmdline(argc, argv)) != 0) {
      print_help();
      return 1;
   }
   /* Check for --version */
   if (cmdl.print_version) {
      print_version();
      return 0;
   }
   /* Check for --help */
   if (cmdl.print_help) {
      print_help();
      return 0;
   }
   /* Switch user when run as root */
   if (is_root_user()) {
      if (!become_another_user(cmdl.runas_user, cmdl.runas_group)) {
         print_stderr(
               "Failed to switch to user %s.%s. Refusing to run as root\n",
               cmdl.runas_user, cmdl.runas_group);
         return -1;
      }
   }
   /* Read config file */
   if (!conf.Read(cmdl.config_file)) {
      return 1;
   }
   /* Setup logging */
   if (strlen(conf.logfile)) {
      vlog.OpenLog(conf.logfile, LOG_INFO);
   }
   /* Sanity check slot number for load command */
   if (cmdl.command == 2) {
      if ((cmdl.slot < 1) || (cmdl.slot > conf.slots)) {
         vlog.Error("%s: Error! invalid slot %d", conf.changer_name, cmdl.slot);
         print_stderr("invalid slot number %d\n", cmdl.slot);
         return 1;
      }
   }
   /* Sanity check drive number for load, unload, and loaded commands */
   if (cmdl.command > 1 && cmdl.command < 5) {
      if (cmdl.drive < 0 || cmdl.drive >= conf.virtual_drives) {
         vlog.Error("%s: Error! invalid drive %d", conf.changer_name,
               cmdl.drive);
         print_stderr("invalid drive number %d\n", cmdl.drive);
         return 1;
      }
   }
   /* Init changer object */
   if (changer.Initialize(conf)) {
      changer.GetLastError(errmsg, sizeof(errmsg));
      vlog.Error("%s: %s", conf.changer_name, errmsg);
      print_stderr("%s\n", errmsg);
      return 1;
   }

   /* Perform command */
   switch (cmdl.command) {
   case 0:
      /* list */
      error_code = do_list_cmd();
      break;
   case 1:
      /* slots */
      error_code = do_slots_cmd();
      break;
   case 2:
      /* load */
      error_code = do_load_cmd();
      break;
   case 3:
      /* unload */
      error_code = do_unload_cmd();
      break;
   case 4:
      /* loaded */
      error_code = do_loaded_cmd();
      break;
   case 5:
      /* initmag  */
      error_code = do_magazine_init(cmdl.init_mag, cmdl.init_mag_num);
      break;
   case 6:
      /* listmags  */
      error_code = do_list_magazines();
      break;
   }

   return error_code;
}
