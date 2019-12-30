/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#include <cstdio>
#include <cstdlib>
#include <string>
#include <cstring>
#include <climits>
#include <unistd.h>
#include <fcntl.h>
#include <regex>
#include <sstream>
#include <stdint.h>
#include <vector>

using namespace std;

/* used by error routines as the name of the program */
char *progname = NULL;

static void usage(
    void);

static void process(
    struct arch *archs,
    uint32_t narchs);

static void write_on_input(
    struct arch *archs,
    uint32_t narchs,
    char *input);

static void setup_object_symbolic_info(
    struct object *object);

static void update_load_commands(
    struct arch *arch,
    uint32_t *header_size);

/* the argument to the -id option */
static char *id = NULL;

/* the arguments to the -change options */
struct changes {
    char *cold;
    char *cnew;
};
static struct changes *changes = NULL;
static uint32_t nchanges = 0;

/* the arguments to the -rpath options */
struct rpaths {
    char *cold;
    char *cnew;
    bool found;
};
static struct rpaths *rpaths = NULL;
static uint32_t nrpaths = 0;

/* the arguments to the -add_rpath options */
struct add_rpaths {
    char *cnew;
};
static struct add_rpaths *add_rpaths = NULL;
static uint32_t nadd_rpaths = 0;

/* the arguments to the -delete_rpath options */
struct delete_rpaths {
    char *cold;
    bool found;
};
static struct delete_rpaths *delete_rpaths = NULL;
static uint32_t ndelete_rpaths = 0;

/*
 * This is a pointer to an array of the original header sizes (mach header and
 * load commands) for each architecture which is used when we are writing on the
 * input file.
 */
static uint32_t *arch_header_sizes = NULL;

/*
 * The -o output option is not enabled as it is not needed and has the
 * unintended side effect of changing the time stamps in LC_ID_DYLIB commands
 * which is not the desired functionality of this command.
 */
#undef OUTPUT_OPTION

const char *version = "https://github.com/dmikushin/install_name_tool";

extern "C"
{
	int patchElfCmdline(int argc, char** argv);
	int patchElfReadRpath(const char* filename, char* rpath, unsigned int* szrpath);
}

/*
 * The install_name_tool allow the dynamic shared library install names of a
 * Mach-O binary to be changed.  For this tool to work when the install names
 * are larger the binary should be built with the ld(1)
 * -headerpad_max_install_names option.
 *
 *    Usage: install_name_tool [-change old new] ... [-rpath old new] ...
 * 	[-add_rpath new] ... [-delete_rpath old] ... [-id name] input
 *
 * The "-change old new" option changes the "old" install name to the "new"
 * install name if found in the binary.
 * 
 * The "-rpath old new" option changes the "old" path name in the rpath to
 * the "new" path name in an LC_RPATH load command in the binary.
 *
 * The "-add_rpath new" option adds an LC_RPATH load command.
 *
 * The "-delete_rpath old" option deletes the LC_RPATH load command with the
 * "old" path name in the binary.
 *
 * The "-id name" option changes the install name in the LC_ID_DYLIB load
 * command for a dynamic shared library.
 */
int
main(
int argc,
char **argv,
char **envp)
{
    int i, j;
    struct arch *archs;
    uint32_t narchs;
    char *input;
    char *output;

	output = NULL;
	progname = argv[0];
	input = NULL;
	archs = NULL;
	narchs = 0;
	for(i = 1; i < argc; i++){
#ifdef OUTPUT_OPTION
	    if(strcmp(argv[i], "-o") == 0){
		if(i + 1 == argc){
		    fprintf(stderr, "missing argument to: %s option", argv[i]);
		    usage();
		}
		if(output != NULL){
		    fprintf(stderr, "more than one: %s option specified", argv[i]);
		    usage();
		}
		output = argv[i+1];
		i++;
	    }
	    else
#endif /* OUTPUT_OPTION */
	    if(strcmp(argv[i], "-id") == 0){
		if(i + 1 == argc){
		    fprintf(stderr, "missing argument to: %s option", argv[i]);
		    usage();
		}
		if(id != NULL){
		    fprintf(stderr, "more than one: %s option specified", argv[i]);
		    usage();
		}
		id = argv[i+1];
		i++;
	    }
	    else if(strcmp(argv[i], "-change") == 0){
		if(i + 2 >= argc){
		    fprintf(stderr, "missing argument(s) to: %s option", argv[i]);
		    usage();
		}
		changes = (struct changes*)realloc(changes,
				     sizeof(struct changes) * (nchanges + 1));
		changes[nchanges].cold = argv[i+1];
		changes[nchanges].cnew = argv[i+2];
		nchanges += 1;
		i += 2;
	    }
	    else if(strcmp(argv[i], "-rpath") == 0){
		if(i + 2 >= argc){
		    fprintf(stderr, "missing argument(s) to: %s option", argv[i]);
		    usage();
		}
		for(j = 0; j < nrpaths; j++){
		    if(strcmp(rpaths[j].cold, argv[i+1]) == 0){
		        if(strcmp(rpaths[j].cnew, argv[i+2]) == 0){
			    fprintf(stderr, "\"-rpath %s %s\" specified more than once",
				   argv[i+1], argv[i+2]);
			    usage();
			}
			fprintf(stderr, "can't specify both \"-rpath %s %s\" and "
			      "\"-rpath %s %s\"", rpaths[j].cold, rpaths[j].cnew,
			      argv[i+1], argv[i+2]);
			usage();
		    }
		    if(strcmp(rpaths[j].cnew, argv[i+1]) == 0 ||
		       strcmp(rpaths[j].cold, argv[i+2]) == 0 ||
		       strcmp(rpaths[j].cnew, argv[i+2]) == 0){
			fprintf(stderr, "can't specify both \"-rpath %s %s\" and "
			      "\"-rpath %s %s\"", rpaths[j].cold, rpaths[j].cnew,
			      argv[i+1], argv[i+2]);
			usage();
		    }
		}
		for(j = 0; j < nadd_rpaths; j++){
		    if(strcmp(add_rpaths[j].cnew, argv[i+1]) == 0 ||
		       strcmp(add_rpaths[j].cnew, argv[i+2]) == 0){
			fprintf(stderr, "can't specify both \"-add_rpath %s\" "
			      "and \"-rpath %s %s\"", add_rpaths[j].cnew,
			      argv[i+1], argv[i+2]);
			usage();
		    }
		}
		for(j = 0; j < ndelete_rpaths; j++){
		    if(strcmp(delete_rpaths[j].cold, argv[i+1]) == 0 ||
		       strcmp(delete_rpaths[j].cold, argv[i+2]) == 0){
			fprintf(stderr, "can't specify both \"-delete_rpath %s\" "
			      "and \"-rpath %s %s\"", delete_rpaths[j].cold,
			      argv[i+1], argv[i+2]);
			usage();
		    }
		}
		rpaths = (struct rpaths*)realloc(rpaths,
				    sizeof(struct rpaths) * (nrpaths + 1));
		rpaths[nrpaths].cold = argv[i+1];
		rpaths[nrpaths].cnew = argv[i+2];
		rpaths[nrpaths].found = false;
		nrpaths += 1;
		i += 2;
	    }
	    else if(strcmp(argv[i], "-add_rpath") == 0){
		if(i + 1 == argc){
		    fprintf(stderr, "missing argument(s) to: %s option", argv[i]);
		    usage();
		}
		for(j = 0; j < nadd_rpaths; j++){
		    if(strcmp(add_rpaths[j].cnew, argv[i+1]) == 0){
			fprintf(stderr, "\"-add_rpath %s\" specified more than once",
			      add_rpaths[j].cnew);
			usage();
		    }
		}
		for(j = 0; j < nrpaths; j++){
		    if(strcmp(rpaths[j].cold, argv[i+1]) == 0 ||
		       strcmp(rpaths[j].cnew, argv[i+1]) == 0){
			fprintf(stderr, "can't specify both \"-rpath %s %s\" and "
			      "\"-add_rpath %s\"", rpaths[j].cold, rpaths[j].cnew,
			      argv[i+1]);
			usage();
		    }
		}
		for(j = 0; j < ndelete_rpaths; j++){
		    if(strcmp(delete_rpaths[j].cold, argv[i+1]) == 0){
			fprintf(stderr, "can't specify both \"-delete_rpath %s\" "
			      "and \"-add_rpath %s\"", delete_rpaths[j].cold,
			      argv[i+1]);
			usage();
		    }
		}
		add_rpaths = (struct add_rpaths*)realloc(add_rpaths,
				sizeof(struct add_rpaths) * (nadd_rpaths + 1));
		add_rpaths[nadd_rpaths].cnew = argv[i+1];
		nadd_rpaths += 1;
		i += 1;
	    }
	    else if(strcmp(argv[i], "-delete_rpath") == 0){
		if(i + 1 == argc){
		    fprintf(stderr, "missing argument(s) to: %s option", argv[i]);
		    usage();
		}
		for(j = 0; j < ndelete_rpaths; j++){
		    if(strcmp(delete_rpaths[j].cold, argv[i+1]) == 0){
			fprintf(stderr, "\"-delete_rpath %s\" specified more than once",
			      delete_rpaths[j].cold);
			usage();
		    }
		}
		for(j = 0; j < nrpaths; j++){
		    if(strcmp(rpaths[j].cold, argv[i+1]) == 0 ||
		       strcmp(rpaths[j].cnew, argv[i+1]) == 0){
			fprintf(stderr, "can't specify both \"-rpath %s %s\" and "
			      "\"-delete_rpath %s\"", rpaths[j].cold,
			      rpaths[j].cnew, argv[i+1]);
			usage();
		    }
		}
		for(j = 0; j < nadd_rpaths; j++){
		    if(strcmp(add_rpaths[j].cnew, argv[i+1]) == 0){
			fprintf(stderr, "can't specify both \"-add_rpath %s\" "
			      "and \"-delete_rpath %s\"", add_rpaths[j].cnew,
			      argv[i+1]);
			usage();
		    }
		}
		delete_rpaths = (struct delete_rpaths*)realloc(delete_rpaths,
				sizeof(struct delete_rpaths) *
					(ndelete_rpaths + 1));
		delete_rpaths[ndelete_rpaths].cold = argv[i+1];
		delete_rpaths[ndelete_rpaths].found = false;
		ndelete_rpaths += 1;
		i += 1;
	    }
	    else{
		if(input != NULL){
		    fprintf(stderr, "more than one input file specified (%s and %s)",
			  argv[i], input);
		    usage();
		}
		input = argv[i];
	    }
	}
	if(input == NULL || (id == NULL && nchanges == 0 && nrpaths == 0 &&
	   nadd_rpaths == 0 && ndelete_rpaths == 0))
	    usage();

	for (int i = 0; i < nchanges; i++){
		const char* argv[] = { "", "--replace-needed", changes[i].cold, changes[i].cnew, input, NULL };
		patchElfCmdline(sizeof(argv) / sizeof(argv[0]) - 1, const_cast<char**>(argv));
	}
	if (id){
		const char* argv[] = { "", "--set-soname", id, input, NULL };
		patchElfCmdline(sizeof(argv) / sizeof(argv[0]) - 1, const_cast<char**>(argv));
	}

	unsigned int szrpath = 0;
	patchElfReadRpath(input, NULL, &szrpath);
	vector<char> vrpath(szrpath);
	patchElfReadRpath(input, reinterpret_cast<char*>(&vrpath[0]), NULL);
	string rpath(reinterpret_cast<char*>(&vrpath[0]), vrpath.size());

	bool modified = false;

	for (int i = 0; i < nrpaths; i++){
		stringstream cold;
		cold << "(^|:)";
		cold << rpaths[i].cold;
		cold << "(:|$)";
 
 		stringstream cnew;
 		cnew << "$1";
 		cnew << rpaths[i].cnew;
 		cnew << "$2";

		while (1)
		{
			string rpathNew = std::regex_replace(rpath, std::regex(cold.str()), cnew.str());
			if (rpathNew != rpath){
				rpath = rpathNew;
				modified = true;
			}
			else break;
		}
	}

	for (int i = 0; i < nadd_rpaths; i++){
		if (rpath.length()) rpath.append(":");
		rpath.append(add_rpaths[i].cnew);
		modified = true;
	}

	for (int i = 0; i < ndelete_rpaths; i++){
		stringstream cold;
		cold << "(^|:)";
		cold << delete_rpaths[i].cold;
		cold << "(:|$)";
 
 		stringstream cnew;
 		cnew << "$1";
 		cnew << "$2";

		while (1)
		{
			string rpathNew = std::regex_replace(rpath, std::regex(cold.str()), cnew.str());
			if (rpathNew != rpath){
				rpath = rpathNew;
				modified = true;
			}
			else break;
		}
	}
	
	if (modified){
	
		// Remove duplicate colons.
		while (1)
		{
			string rpathNew = std::regex_replace(rpath, std::regex("::"), ":");
			if (rpathNew != rpath){
				rpath = rpathNew;
				modified = true;
			}
			else break;
		}
		
		// If only a single colon remaining, pass an empty RPATH.
		if (rpath == ":") rpath = "";
	
		const char* crpath = rpath.c_str();
		const char* argv[] = { "", "--set-rpath", crpath, input, NULL };
		patchElfCmdline(sizeof(argv) / sizeof(argv[0]) - 1, const_cast<char**>(argv));
	}

	return(EXIT_SUCCESS);
}

/*
 * usage() prints the current usage message and exits indicating failure.
 */
static
void
usage(
void)
{
	fprintf(stderr, "Usage: %s [-change old new] ... [-rpath old new] ... "
			"[-add_rpath new] ... [-delete_rpath old] ... "
			"[-id name] input"
#ifdef OUTPUT_OPTION
		" [-o output]"
#endif /* OUTPUT_OPTION */
		"\n", progname);
	exit(EXIT_FAILURE);
}

