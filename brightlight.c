/* brightlight v2-rc2 - change the screen backlight brightness on Linux systems
** Copyright (C) 2016 David Miller <multiplexd@gmx.com>
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

/*
** This program requires libbsd <libbsd.freedesktop.org> or a BSD-compatible 
** implementation of strlcpy() and strlcat().
**
** To compile:
**
**   $ gcc -o brightlight brightlight.c -lbsd
**
** (assuming you use gcc of course; clang works as well). This program can 
** also be statically linked by adding the -static flag.
*/

#include "brightlight.h"

int main(int argc, char* argv[]) {

   argv0 = argv[0];

   parse_args(argc, argv);

   validate_control_directory();

   maximum = get_value_from_file("/max_brightness");

   if(set_backlight) {
      validate_args();
   }

   if(get_backlight) {
      read_backlight_brightness();
   } else if(set_backlight) {
      write_backlight_brightness();
   } else if(max_brightness) {
      read_maximum_brightness();
   } else if(inc_brightness || dec_brightness) {
      change_existing_brightness();
   } else {
      throw_error(ERR_PARSE_OPTS, "");
   }

   exit(0);
}

void change_existing_brightness() {
   unsigned int oldval;
   unsigned int current;
   unsigned int val_to_write;
   char* out_string_end;
   char* out_string_filler;
   
   current = get_value_from_file("/brightness");

   if(inc_brightness) { 
      validate_increment(current);      
   } else if(dec_brightness) {
      validate_decrement((int) current);
   }

   if(inc_brightness) {
      if(values_as_percentages) {
         val_to_write = current + ((delta_brightness * maximum) / 100);
         oldval = (current * 100) / maximum;
         out_string_end = "%.";
         out_string_filler = "% ";
      } else {
         val_to_write = current + delta_brightness;
         oldval = current;
         out_string_end = ".";
         out_string_filler = " ";
      }
   } else if(dec_brightness){
      if(values_as_percentages) {
         val_to_write = current - ((delta_brightness * maximum) / 100);
         oldval = (current * 100) / maximum;
         out_string_end = "%.";
         out_string_filler = "% ";
      } else {
         val_to_write = current - delta_brightness;
         oldval = current;
         out_string_end = ".";
         out_string_filler = " ";
      }
   }

   write_brightness_to_file(val_to_write);

   if(inc_brightness) {
      printf("Changed backlight brightness: %d%s=> %d%s\n", oldval, out_string_filler, oldval + delta_brightness, out_string_end);      
   } else if(dec_brightness) {
      printf("Changed backlight brightness: %d%s=> %d%s\n", oldval, out_string_filler, oldval - delta_brightness, out_string_end);
   }
   
   return;
}

unsigned int get_value_from_file(char* path_suffix) {
   int value = -1;
   FILE* file_to_read;
   char path[MAX_PATH_LEN + EXTRA_PATH_LEN];

   strlcpy(path, backlight_path, MAX_PATH_LEN);
   strlcat(path, path_suffix, MAX_PATH_LEN + EXTRA_PATH_LEN);

   file_to_read = fopen(path, "r");
   if(file_to_read == NULL)
      throw_error(ERR_OPEN_FILE, path_suffix);

   fscanf(file_to_read, "%d", &value);
   if(value < 0)
      throw_error(ERR_READ_FILE, path_suffix);

   fclose(file_to_read);

   return (unsigned int) value;
}

void parse_args(int argc, char* argv[]) {
   /*
   ** This function, specifically the conditional loop and immediately following 
   ** if statement, is based on the function parse_args() in thttpd.c of thttpd-2.27 
   ** by Jef Poskanzer (http://www.acme.com/software/thttpd).
   */
   int argn, conflicting_args;
   char* cmdline_brightness;

   set_backlight = 0;
   get_backlight = 0;
   max_brightness = 0;
   values_as_percentages = 0;
   brightness = 0;
   strlcpy(backlight_path, BACKLIGHT_PATH, MAX_PATH_LEN);     /* Use the compiled-in default path unless told otherwise */
   conflicting_args = 0;

   if(argc == 1)
      throw_error(ERR_NO_OPTS, "");

   argn = 1;
   while(argn < argc && argv[argn][0] == '-') {
      if(strcmp(argv[argn], "-v") == 0) {
         version();
         exit(0);
      } else if(strcmp(argv[argn], "-h") == 0) {
         usage();
         exit(0);
      } else if(strcmp(argv[argn], "-r") == 0) {
         if(conflicting_args)
	    throw_error(ERR_OPT_CONFLICT, "");
         get_backlight = 1;
         conflicting_args = 1;
      } else if(strcmp(argv[argn], "-w") == 0 && argn + 1 < argc) {
         if(conflicting_args) 
	    throw_error(ERR_OPT_CONFLICT, "");
	 argn++;
         set_backlight = 1;
         cmdline_brightness = argv[argn];
         conflicting_args = 1;
      } else if(strcmp(argv[argn], "-f") == 0 && argn + 1 < argc) {
         argn++;
         strlcpy(backlight_path, argv[argn], MAX_PATH_LEN);
      } else if(strcmp(argv[argn], "-p") == 0) {
         values_as_percentages = 1;
      } else if(strcmp(argv[argn], "-m") == 0) {
         if(conflicting_args) 
	    throw_error(ERR_OPT_CONFLICT, "");
         max_brightness = 1;
         conflicting_args = 1;
      } else if(strcmp(argv[argn], "-i") == 0 && argn + 1 < argc) {
	 if(conflicting_args) 
	    throw_error(ERR_OPT_CONFLICT, "");
	 inc_brightness = 1;
	 argn++;
	 cmdline_brightness = argv[argn];
	 conflicting_args = 1;
      } else if(strcmp(argv[argn], "-d") == 0 && argn + 1 < argc) {
	 if(conflicting_args) 
	    throw_error(ERR_OPT_CONFLICT, "");
	 dec_brightness = 1;
	 argn++;
	 cmdline_brightness = argv[argn];
	 conflicting_args = 1;
      } else {
	 throw_error(ERR_PARSE_OPTS, "");
      }
      argn++;
   }
   if(argn != argc) 
      throw_error(ERR_PARSE_OPTS, "");

   if(set_backlight)
      brightness = parse_cmdline_int(cmdline_brightness);

   if(inc_brightness || dec_brightness)
      delta_brightness = parse_cmdline_int(cmdline_brightness);
   return;
}

unsigned int parse_cmdline_int(char* arg_to_parse) {
   int character = 0;

   while(arg_to_parse[character] != '\0') {
      if(character >= 5 || isdigit(arg_to_parse[character]) == 0) 
	 throw_error(ERR_INVAL_OPT, "");
      character++;
   }

   return (unsigned int) atoi(arg_to_parse);
}

void read_backlight_brightness() {
   unsigned int outval;
   char* out_string_end;

   brightness = get_value_from_file("/brightness");

   if(values_as_percentages) {
      outval = (brightness * 100) / maximum;
      out_string_end = "%.";
   } else {
      outval = brightness;
      out_string_end = ".";
   }

   printf("Current backlight brightness is: %d%s\n", outval, out_string_end);

   return;
}

void read_maximum_brightness() {
   unsigned int outval;
   char* out_string_end;

   if(values_as_percentages) {
      outval = (maximum * 100) / maximum;
      out_string_end = "%.";
   } else {
      outval = maximum;
      out_string_end = ".";
   }

   printf("Maximum backlight brightness is: %d%s\n", outval, out_string_end);

   return;
}

void throw_error(enum errors err, char* opt_arg) {

   switch(err) {
   case ERR_OPEN_FILE:
      fprintf(stderr, "Error occured while trying to open %s file.\n", opt_arg);
      break;
   case ERR_READ_FILE:
      fprintf(stderr, "Could not read from %s file.\n", opt_arg);
      break;
   case ERR_WRITE_FILE:
      fputs("Could not write brightness to brightness file.\n", stderr);
      break;
   case ERR_NO_OPTS:
      fputs("No options specified. Pass the -h flag for help.\n", stderr);
      break;
   case ERR_OPT_CONFLICT:
      fputs("Conflicting options given! Pass the -h flag for help.\n", stderr);
      break;
   case ERR_PARSE_OPTS:
      fputs("Error parsing options. Pass the -h flag for help.\n", stderr);
      break;
   case ERR_INVAL_OPT:
      fputs("Invalid argument. Pass the -h flag for help.\n", stderr);
      break;
   case ERR_FILE_IS_NOT_DIR:
      fprintf(stderr, "%s is not a directory.\n", opt_arg);
      break;
   case ERR_ACCES_ON_DIR:
      fprintf(stderr, "Could not access %s: Permission denied.\n", opt_arg);
      break;
   case ERR_CONTROL_DIR:
      fputs("Could not access control directory.\n", stderr);
      break;
   case ERR_FILE_ACCES:
      fprintf(stderr, "Control directory exists but could not find %s control file.\n", opt_arg);
      break;
   }

   exit(1);
}

void usage() {
   printf("Usage: %s [OPTIONS]\n", argv0);
   printf("\
Options:\n\
\n\
      -v         Print program version and exit.\n\
      -h         Show this help message.\n\
      -p         Read or write the brightness level as a percentage (0 to 100)\n\
                 instead of the internal scale the kernel uses (such as e.g. 0\n\
                 to 7812).\n\
      -r         Read the backlight brightness level.\n\
      -w <val>   Set the backlight brightness level to <val>, where <val> is a\n\
                 a positive integer.\n\
      -i <val>   Increment the backlight brightness level by <val>, where\n\
                 <val> is a positive integer.\n\
      -d <val>   Decrement the backlight brightness level by <val>, where\n\
                 <val> is a positive integer.\n\
      -f <path>  Specify alternative path to backlight control directory, such\n\
                 as \"/sys/class/backlight/intel_backlight/\". Must be an\n\
                 absolute path with a trailing slash.\n\
      -m         Show maximum brightness level of the screen backlight on the \n\
                 kernel's scale. The compile-time default control directory is\n\
                 used if -f is not specified. The -p flag is ignored when this\n\
                 option is specified.\n\n");

   printf("The flags -r, -w, -m, -i and -d are mutually exclusive, however one of the \nfive is required.\n");

   return;
}

void validate_args() {
/* Don't check if brightness is an int, already done by parse_cmdline_int() */

   if(values_as_percentages) {
      if(brightness < 0 || brightness > 100) 
	 throw_error(ERR_INVAL_OPT, "");
   } else {
      if(brightness < 0 || brightness > maximum) 
	 throw_error(ERR_INVAL_OPT, "");
   }

   return;
}

void validate_control_directory() {
   DIR* control_dir;
   char path[MAX_PATH_LEN + EXTRA_PATH_LEN];

   control_dir = opendir(backlight_path);

   if(control_dir == NULL) {
      if(errno == ENOTDIR) 
	 throw_error(ERR_FILE_IS_NOT_DIR, backlight_path);
      else if(errno == EACCES) 
         throw_error(ERR_ACCES_ON_DIR, backlight_path);
      else 
	 throw_error(ERR_CONTROL_DIR, "");
   }

   closedir(control_dir);

   strlcpy(path, backlight_path, MAX_PATH_LEN);
   strlcat(path, "/brightness", EXTRA_PATH_LEN);

   if(access(path, F_OK) != 0)
      throw_error(ERR_FILE_ACCES, "brightness");

   strlcpy(path, backlight_path, MAX_PATH_LEN);
   strlcat(path, "/max_brightness", EXTRA_PATH_LEN);

   if(access(path, F_OK) != 0) 
      throw_error(ERR_FILE_ACCES, "max_brightness");
      
   return;
}

void validate_decrement(unsigned int reference_value) {

   if(values_as_percentages) {
      int current = (reference_value * 100) / maximum;
      if(current - (int) delta_brightness < 0) 
	 throw_error(ERR_INVAL_OPT, "");
   } else {
      if((int) reference_value - (int) delta_brightness < 0) 
	 throw_error(ERR_INVAL_OPT, "");
   }

   return;
}

void validate_increment(unsigned int reference_value) {

   if(values_as_percentages) {
      unsigned int current = (reference_value * 100) / maximum;
      if(current + delta_brightness > 100) 
	 throw_error(ERR_INVAL_OPT, "");
   } else {
      if(reference_value + delta_brightness > maximum) 
	 throw_error(ERR_INVAL_OPT, "");
   }

   return;
}

void version() {
   printf("%s v%s\n", PROGRAM_NAME, PROGRAM_VERSION);
   puts("Copyright (C) 2016 David Miller <multiplexd@gmx.com>");
   printf("\
This is free software under the terms of the GNU General Public License, \n\
version 2 or later. You are free to use, modify and redistribute it, however \n\
there is NO WARRANTY; please see <https://gnu.org/licenses/gpl.html> for \n\
further information.\n");
}

void write_backlight_brightness() {
   unsigned int oldval;
   unsigned int val_to_write;
   unsigned int current;
   char* out_string_end;
   char* out_string_filler;

   current = get_value_from_file("/brightness");

   if(values_as_percentages) {
      val_to_write = (brightness * maximum) / 100;
      oldval = (current * 100) / maximum;
      out_string_end = "%.";
      out_string_filler = "% ";
   } else {
      val_to_write = brightness;
      oldval = current;
      out_string_end = ".";
      out_string_filler = " ";
   }

   write_brightness_to_file(val_to_write);

   printf("Changed backlight brightness: %d%s=> %d%s\n", oldval, out_string_filler, brightness, out_string_end);      

   return;
}

void write_brightness_to_file(unsigned int bright) {
   FILE* brightness_file;
   char path[MAX_PATH_LEN + EXTRA_PATH_LEN];

   strlcpy(path, backlight_path, MAX_PATH_LEN);
   strlcat(path, "/brightness", MAX_PATH_LEN + EXTRA_PATH_LEN);

   brightness_file = fopen(path, "w");
   if(brightness_file == NULL) 
      throw_error(ERR_OPEN_FILE, "brightness");

   if(fprintf(brightness_file, "%d", bright) < 0) 
      throw_error(ERR_WRITE_FILE, "");

   fclose(brightness_file);

   return;
}

