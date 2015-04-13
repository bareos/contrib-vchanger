/* vchanger.cpp
 *
 *  This file is part of the vchanger package
 *
 *  vchanger copyright (C) 2008-2014 Josh Fisher
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

#include "config.h"
#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_CTYPE_H
#include <ctype.h>
#endif
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "util.h"
#include "compat_defs.h"
#include "loghandler.h"
#include "diskchanger.h"

DiskChanger changer;

/*-------------------------------------------------
 *  Commands
 * ------------------------------------------------*/
#define NUM_AUTOCHANGER_COMMANDS 9
static char autochanger_command[NUM_AUTOCHANGER_COMMANDS][32] = { "list", "slots", "load",
      "unload", "loaded", "listall", "listmags", "createvols", "refresh" };
#define CMD_LIST        0
#define CMD_SLOTS       1
#define CMD_LOAD        2
#define CMD_UNLOAD      3
#define CMD_LOADED      4
#define CMD_LISTALL     5
#define CMD_LISTMAGS    6
#define CMD_CREATEVOLS  7
#define CMD_REFRESH     8

/*-------------------------------------------------
 *  Command line parameters
 * ------------------------------------------------*/
typedef struct _cmdparams_s
{
   bool print_version;
   bool print_help;
   int command;
   int slot;
   int drive;
   int mag_bay;
   int count;
   tString label_prefix;
   tString pool;
   tString runas_user;
   tString runas_group;
   tString config_file;
   tString archive_device;
} CMDPARAMS;
CMDPARAMS cmdl;

/*-------------------------------------------------
 *  Function to print version info to stdout
 *------------------------------------------------*/
static void print_version(void)
{
   fprintf(stdout, "%s version %s\n", PACKAGE_NAME, PACKAGE_VERSION);
   fprintf(stdout, "\n%s.\n", COPYRIGHT_NOTICE);
}

/*-------------------------------------------------
 *  Function to print command help to stdout
 *------------------------------------------------*/
static void print_help(void)
{
   fprintf(stdout, "vchanger version %s\n\n", PACKAGE_VERSION);
   fprintf(stdout, "USAGE:\n\n"
      "  vchanger [options] config_file command slot device drive\n"
      "    Perform Bacula Autochanger API command for virtual\n"
      "    changer defined by vchanger configuration file\n"
      "    'config_file' using 'slot', 'device', and 'drive'\n"
      "  vchanger [options] config_file LISTMAGS\n"
      "    vchanger extension to list info on all defined magazines.\n"
      "  vchanger [options] config_file CREATEVOLS mag_ndx count [start] [CREATEVOLS options]\n"
      "    vchanger extension to create 'count' empty volume files on the magazine at"
      "    index 'mag_ndx'. If specified, 'start' is the lowest integer to use when"
      "    appending integers to the label prefix when generating volume names.\n"
      "  vchanger [options] config_file REFRESH\n"
      "  vchanger --version\n"
      "    print version info\n"
      "  vchanger --help\n"
      "    print help\n"
      "\nGeneral options:\n"
      "    -u, --user=uid       user to run as (when invoked by root)\n"
      "    -g, --group=gid      group to run as (when invoked by root)\n"
      "\nCREATEVOLS command options:\n"
      "    -l, --label=string   string to use as a prefix for determining the\n"
      "                         barcode label of the volume files created. Labels\n"
      "                         will be of the form 'string'_N, where N is an\n"
      "                         integer. By default the prefix will be generated\n"
      "                         using the changer name and the position of the\n"
      "                         magazine's declaration in the configuration file.\n"
      "    --pool=string        Overrides the default pool, defined in the vchanger\n"
      "                         config file, that new volumes should be placed into\n"
      "                         when labeling newly created volumes.\n"
      "\nReport bugs to %s.\n", PACKAGE_BUGREPORT);
}

/*-------------------------------------------------
 *  Function to parse command line parameters
 *------------------------------------------------*/
#define LONGONLYOPT_VERSION   0
#define LONGONLYOPT_HELP      1
#define LONGONLYOPT_POOL      2

static int parse_cmdline(int argc, char *argv[])
{
   int c, ndx = 0;
   tString tmp;
   struct option options[] = { { "version", 0, 0, LONGONLYOPT_VERSION },
         { "help", 0, 0, LONGONLYOPT_HELP },
         { "user", 1, 0, 'u' },
         { "group", 1, 0, 'g' },
         { "label", 1, 0, 'l' },
         { "pool", 1, 0, LONGONLYOPT_POOL },
         { 0, 0, 0, 0 } };

   cmdl.print_version = false;
   cmdl.print_help = false;
   cmdl.command = 0;
   cmdl.slot = 0;
   cmdl.drive = 0;
   cmdl.mag_bay = 0;
   cmdl.count = 0;
   cmdl.label_prefix.clear();
   cmdl.pool.clear();
   cmdl.runas_user.clear();
   cmdl.runas_group.clear();
   cmdl.config_file.clear();
   cmdl.archive_device.clear();
   /* process the command line */
   for (;;) {
      c = getopt_long(argc ,argv, "u:g:l:", options, NULL);
      if (c == -1) break;
      switch (c) {
      case LONGONLYOPT_VERSION:
         cmdl.print_version = true;
         cmdl.print_help = false;
         return 0;
      case LONGONLYOPT_HELP:
         cmdl.print_version = false;
         cmdl.print_help = true;
         return 0;
      case 'u':
         cmdl.runas_user = optarg;
         break;
      case 'g':
         cmdl.runas_group = optarg;
         break;
      case 'l':
         cmdl.label_prefix = optarg;
         break;
      case LONGONLYOPT_POOL:
         cmdl.pool = optarg;
         break;
      default:
         fprintf(stderr, "unknown option %s\n", optarg);
         return -1;
      }
   }

   /* process positional params */
   ndx = optind;
   /* First parameter is the vchanger config file path */
   if (ndx >= argc) {
      fprintf(stderr, "missing parameter 1 (config_file)\n");
      return -1;
   }
   cmdl.config_file = argv[ndx];
   /* Second parameter is the command */
   ++ndx;
   if (ndx >= argc) {
      fprintf(stderr, "missing parameter 2 (command)\n");
      return -1;
   }
   tmp = argv[ndx];
   tToLower(tStrip(tmp));
   for (cmdl.command = 0; cmdl.command < NUM_AUTOCHANGER_COMMANDS; cmdl.command++) {
      if (tmp == autochanger_command[cmdl.command]) break;
   }
   if (cmdl.command >= NUM_AUTOCHANGER_COMMANDS) {
      fprintf(stderr, "'%s' is not a recognized command", argv[ndx]);
      return -1;
   }
   /* Make sure only CREATEVOLS command has -l flag */
   if (!cmdl.label_prefix.empty() && cmdl.command != CMD_CREATEVOLS) {
      fprintf(stderr, "flag -l not valid for this command\n");
      return -1;
   }
   /* Make sure only CREATEVOLS command has --pool flag */
   if (!cmdl.pool.empty() && cmdl.command != CMD_CREATEVOLS) {
      fprintf(stderr, "flag --pool not valid for this command\n");
      return -1;
   }
   /* Check param 3 exists */
   ++ndx;
   if (ndx >= argc) {
      /* Only 2 parameters given */
      switch (cmdl.command) {
      case CMD_LIST:
      case CMD_LISTALL:
      case CMD_SLOTS:
      case CMD_LISTMAGS:
      case CMD_REFRESH:
         return 0;   /* OK, because these commands only need 2 parameters */
      case CMD_CREATEVOLS:
         fprintf(stderr, "missing parameter 3 (magazine index)\n");
         break;
      default:
         fprintf(stderr, "missing parameter 3 (slot number)\n");
         break;
      }
      return -1;
   }
   /* Process parameter 3 */
   switch (cmdl.command) {
   case CMD_LIST:
   case CMD_LISTALL:
   case CMD_SLOTS:
   case CMD_LISTMAGS:
   case CMD_REFRESH:
      return 0;  /* These commands only need 2 params, so ignore extraneous */
   case CMD_CREATEVOLS:
      /* Param 3 for CREATEVOLS command is magazine index */
      cmdl.mag_bay = (int)strtol(argv[ndx], NULL, 10);
      if (cmdl.mag_bay < 0) {
         fprintf(stderr, "invalid magazine index in parameter 3\n");
         return -1;
      }
      break;
   case CMD_LOADED:
      /* slot is ignored for LOADED command, so just set to 1 */
      cmdl.slot = 1;
      break;
   default:
      /* Param 3 for all other commands is the slot number */
      cmdl.slot = (int)strtol(argv[ndx], NULL, 10);
      if (cmdl.slot < 1) {
         fprintf(stderr, "invalid slot number in parameter 3\n");
         return -1;
      }
      break;
   }
   /* Check param 4 exists */
   ++ndx;
   if (ndx >= argc) {
      /* Only 3 parameters given */
      switch (cmdl.command) {
      case CMD_CREATEVOLS:
         fprintf(stderr, "missing parameter 4 (count)\n");
         break;
      default:
         fprintf(stderr, "missing parameter 4 (archive device)\n");
         break;
      }
      return -1;
   }
   /* Process param 4 */
   switch (cmdl.command) {
   case CMD_CREATEVOLS:
      /* Param 4 for CREATEVOLS command is volume count */
      cmdl.count = (int)strtol(argv[ndx], NULL, 10);
      if (cmdl.count <= 0 ) {
         fprintf(stderr, "invalid count in parameter 4\n");
         return -1;
      }
      break;
   default:
      /* Param 4 for all other commands is the archive device path */
      cmdl.archive_device = argv[ndx];
      break;
   }
   /* Check param 5 exists */
   ++ndx;
   if (ndx >= argc) {
      /* Only 4 parameters given */
      switch (cmdl.command) {
      case CMD_CREATEVOLS:
         cmdl.slot = -1;
         return 0; /* OK, because parameter 5 optional */
      default:
         fprintf(stderr, "missing parameter 5 (drive index)\n");
         break;
      }
      return -1;
   }
   switch (cmdl.command) {
   case CMD_CREATEVOLS:
      cmdl.slot = (int)strtol(argv[ndx], NULL, 10);
      if (cmdl.slot < 0) cmdl.slot = -1;
      break;
   default:
      /* Param 5 for all other commands is drive index number */
      if (!isdigit(argv[ndx][0])) {
         fprintf(stderr, "invalid drive index in parameter 5\n");
         return -1;
      }
      cmdl.drive = (int)strtol(argv[ndx], NULL, 10);
      if (cmdl.drive < 0) {
         fprintf(stderr, "invalid drive index in parameter 5\n");
         return -1;
      }
      break;
   }

   /*  note that any extraneous parameters are simply ignored */
   return 0;
}


/*-------------------------------------------------
 *   LIST Command
 * Prints a line on stdout for each autochanger slot that contains a
 * volume file, even if that volume is currently loaded in a drive.
 * Output is of the form:
 *       s:barcode
 * where 's' is the one-based virtual slot number and 'barcode' is the barcode
 * label of the volume in the slot. The volume in the slot is a file on one
 * of the changer's magazines. A magazine is a directory, which is usually the
 * mountpoint of a filesystem partition. The changer has one or more
 * magazines, each of which may or may not be attached. Each volume file on
 * each magazine is mapped to a virtual slot. The barcode is the volume filename.
 *------------------------------------------------*/
static int do_list_cmd()
{
   int slot, num_slots = changer.NumSlots();

   /* Print all slot numbers, adding volume labels for non-empty slots */
   for (slot = 1; slot <= num_slots; slot++) {
      if (changer.SlotEmpty(slot)) {
         fprintf(stdout, "%d:\n", slot);
      } else {
         fprintf(stdout, "%d:%s\n", slot, changer.GetVolumeLabel(slot));
      }
   }
   log.Info("  SUCCESS sent list to stdout");
   return 0;
}


/*-------------------------------------------------
 *   SLOTS Command
 * Prints the number of virtual slots the changer has
 *------------------------------------------------*/
static int do_slots_cmd()
{
   fprintf(stdout, "%d\n", changer.NumSlots());
   log.Info("  SUCCESS reporting %d slots", changer.NumSlots());
   return 0;
}


/*-------------------------------------------------
 *   LOAD Command
 * Loads the volume file mapped to a virtual slot into a virtual drive
 *------------------------------------------------*/
static int do_load_cmd()
{
   if (changer.LoadDrive(cmdl.drive, cmdl.slot)) {
      fprintf(stderr, "%s\n", changer.GetErrorMsg());
      log.Error("  ERROR loading slot %d into drive %d", cmdl.slot, cmdl.drive);
      return 1;
   }
   log.Info("  SUCCESS loading slot %d into drive %d", cmdl.slot, cmdl.drive);
   return 0;
}


/*-------------------------------------------------
 *   UNLOAD Command
 * Unloads the volume in a virtual drive
 *------------------------------------------------*/
static int do_unload_cmd()
{
   if (changer.UnloadDrive(cmdl.drive)) {
      fprintf(stderr, "%s\n", changer.GetErrorMsg());
      log.Error("  ERROR unloading slot %d from drive %d", cmdl.slot, cmdl.drive);
      return 1;
   }
   log.Info("  SUCCESS unloading slot %d from drive %d", cmdl.slot, cmdl.drive);
   return 0;
}


/*-------------------------------------------------
 *   LOADED Command
 * Prints the virtual slot number of the volume file currently loaded
 * into a virtual drive, or zero if the drive is unloaded.
 *------------------------------------------------*/
static int do_loaded_cmd()
{
   int slot = changer.GetDriveSlot(cmdl.drive);
   if (slot < 0) slot = 0;
   fprintf(stdout, "%d\n", slot);
   log.Info("  SUCCESS reporting drive %d loaded from slot %d", cmdl.drive, slot);
   return 0;
}

/*-------------------------------------------------
 *   LISTALL Command
 * Prints state of drives (loaded or empty), followed by state
 * of virtual slots (full or empty).
 *------------------------------------------------*/
static int do_list_all()
{
   int n, s, num_slots = changer.NumSlots();

   /* Print drive state info */
   for (n = 0; n < changer.NumDrives(); n++) {
      if (changer.DriveEmpty(n)) {
         fprintf(stdout, "D:%d:E\n", n);
      } else {
         s = changer.GetDriveSlot(n);
         fprintf(stdout, "D:%d:F:%d:%s\n", n, s,
               changer.GetVolumeLabel(s));
      }
   }
   /* Print slot state info */
   for (n = 1; n <= num_slots; n++) {
      if (changer.SlotEmpty(n)) {
         fprintf(stdout, "S:%d:E\n", n);
      } else {
         if (changer.GetSlotDrive(n) < 0)
            fprintf(stdout, "S:%d:F:%s\n", n, changer.GetVolumeLabel(n));
         else
            fprintf(stdout, "S:%d:E\n", n);
      }
   }
   log.Info("  SUCCESS sent listall to stdout");
   return 0;
}


/*-------------------------------------------------
 *   LISTMAGS (List Magazines) Command
 * Prints a listing of all magazine bays and info on the magazine
 * (if any) each bay contains.
 *------------------------------------------------*/
static int do_list_magazines()
{
   int n;

   if (changer.NumMagazines() == 0) {
      fprintf(stdout, "No magazines are defined\n");
      log.Info("  SUCCESS no magazines are defined");
      return 0;
   }
   for (n = 0; n < changer.NumMagazines(); n++) {
      if (changer.MagazineEmpty(n)) {
         fprintf(stdout, "%d:::\n", n);
      } else {
         fprintf(stdout, "%d:%d:%d:%s\n", n, changer.GetMagazineSlots(n),
               changer.GetMagazineStartSlot(n), changer.GetMagazineMountpoint(n));
      }
   }
   log.Info("  SUCCESS listing magazine info to stdout");
   return 0;
}

/*-------------------------------------------------
 *   CREATEVOLS (Create Volumes) Command
 * Creates volume files on the specified magazine
 *------------------------------------------------*/
static int do_create_vols()
{
   /* Create new volume files on magazine */
   if (changer.CreateVolumes(cmdl.mag_bay, cmdl.count, cmdl.slot, cmdl.label_prefix.c_str())) {
      fprintf(stderr, "%s\n", changer.GetErrorMsg());
      log.Error("  ERROR");
      return -1;
   }
   fprintf(stdout, "Created %d volume files on magazine %d\n",
           cmdl.count, cmdl.mag_bay);
   log.Info("  SUCCESS");
   return 0;
}



/* -------------  Main  -------------------------*/

int main(int argc, char *argv[])
{
   int rc;
   FILE *fs = NULL;
   int32_t error_code;

#ifdef HAVE_LOCALE_H
   setlocale(LC_ALL, "");
#endif

   /* Log initially to stderr */
   log.OpenLog(stderr, LOG_ERR);
   /* parse the command line */
   if ((error_code = parse_cmdline(argc, argv)) != 0) {
      print_help();
      return 1;
   }
   /* Check for --version flag */
   if (cmdl.print_version) {
      print_version();
      return 0;
   }
   /* Check for --help flag */
   if (cmdl.print_help) {
      print_help();
      return 0;
   }
   /* Read vchanger config file */
   if (!conf.Read(cmdl.config_file)) {
      return 1;
   }
   /* User:group from cmdline overrides config file values */
   if (cmdl.runas_user.size()) conf.user = cmdl.runas_user;
   if (cmdl.runas_group.size()) conf.group = cmdl.runas_group;
   /* Pool from cmdline overrides config file */
   if (!cmdl.pool.empty()) conf.def_pool = cmdl.pool;
   /* If root, try to run as configured user:group */
   rc = drop_privs(conf.user.c_str(), conf.group.c_str());
   if (rc) {
      fprintf(stderr, "Error %d attempting to run as user '%s'", rc, conf.user.c_str());
      return 1;
   }
   /* Start logging to log file specified in configuration file */
   if (!conf.logfile.empty()) {
      fs = fopen(conf.logfile.c_str(), "a");
      if (fs == NULL) {
         fprintf(stderr, "Error opening opening log file\n");
         return 1;
      }
      log.OpenLog(fs, conf.log_level);
   }
   /* Validate and commit configuration parameters */
   if (!conf.Validate()) {
      return 1;
   }
   /* Initialize changer. A lock file is created to serialize access
    * to the changer. As a result, changer initialization may block
    * for up to 30 seconds, and may fail if a timeout is reached */
   if (changer.Initialize()) {
      fprintf(stderr, "%s\n", changer.GetErrorMsg());
      return 1;
   }

   /* Perform command */
   switch (cmdl.command) {
   case CMD_LIST:
      log.Debug("==== preforming LIST command pid=%d", getpid());
      error_code = do_list_cmd();
      break;
   case CMD_SLOTS:
      log.Debug("==== preforming SLOTS command pid=%d", getpid());
      error_code = do_slots_cmd();
      break;
   case CMD_LOAD:
      log.Debug("==== preforming LOAD command pid=%d", getpid());
      error_code = do_load_cmd();
      break;
   case CMD_UNLOAD:
      log.Debug("==== preforming UNLOAD command pid=%d", getpid());
      error_code = do_unload_cmd();
      break;
   case CMD_LOADED:
      log.Debug("==== preforming LOADED command pid=%d", getpid());
      error_code = do_loaded_cmd();
      break;
   case CMD_LISTALL:
      log.Debug("==== preforming LISTALL command pid=%d", getpid());
      error_code = do_list_all();
      break;
   case CMD_LISTMAGS:
      log.Debug("==== preforming LISTMAGS command pid=%d", getpid());
      error_code = do_list_magazines();
      break;
   case CMD_CREATEVOLS:
      log.Debug("==== preforming CREATEVOLS command pid=%d", getpid());
      error_code = do_create_vols();
      break;
   case CMD_REFRESH:
      log.Debug("==== preforming REFRESH command pid=%d", getpid());
      error_code = 0;
      log.Info("  SUCCESS pid=%d", getpid());
      break;
   }

   /* If there was an error, then exit */
   if (error_code) {
      changer.Unlock();
      return error_code;
   }

   /* If not updating Bacula, then exit */
   if (conf.bconsole.empty()) {
      /* Bacula interaction via bconsole is disabled, so log warnings */
      if (changer.NeedsUpdate())
         log.Error("WARNING! 'update slots' needed in bconsole pid=%d", getpid());
      if (changer.NeedsLabel())
         log.Error("WARNING! 'label barcodes' needed in bconsole pid=%d", getpid());
      changer.Unlock();
      return 0;
   }

   /* Update Bacula via bconsole */
#ifndef HAVE_WINDOWS_H
   changer.UpdateBacula();
#else
   /* Auto-update of bacula not working for Windows */
   if (changer.NeedsUpdate())
      log.Error("WARNING! 'update slots' needed in bconsole");
   if (changer.NeedsLabel())
      log.Error("WARNING! 'label barcodes' needed in bconsole");
   return 0;
#endif

   changer.Unlock();
   return 0;
}
