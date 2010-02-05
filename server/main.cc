/*
 * Copyright (c) 2010 Stephen Williams (steve@icarus.com)
 *
 *    This source code is free software; you can redistribute it
 *    and/or modify it in source code form under the terms of the GNU
 *    General Public License as published by the Free Software
 *    Foundation; either version 2 of the License, or (at your option)
 *    any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

# include  <iostream>

using namespace std;

# include  <stdlib.h>
# include  <stdio.h>
# include  <unistd.h>
# include  "priv.h"
# include  <assert.h>

int main(int argc, char*argv[])
{
      list<const char*> config_paths;
      const char*trace_path = 0;
      int opt;

      while ( (opt = getopt(argc, argv, "c:t:")) != -1 ) {
	    switch (opt) {
		case 'c':
		  config_paths .push_back(optarg);
		  break;
		case 't':
		  trace_path = optarg;
		  break;
		default:
		  assert(0);
		  break;
	    }
      }

      if (config_paths.size() == 0) {
	    cerr << "Need a config file. Use -c <file> to specify." << endl;
	    return 1;
      }

	/* Initialize the server... */
      service_init(trace_path);

	/* Parse the config files... */
      for (list<const char*>::iterator idx = config_paths.begin()
		 ; idx != config_paths.end() ; idx ++ ) {

	    const char*config_path = *idx;
	    FILE*cfg = fopen(config_path, "r");
	    if (cfg == 0) {
		  cerr << "Unable to open " << config_path << endl;
		  return 2;
	    }

	    int rc = config_file(cfg);
	    if (rc != 0)
		  return 3;
      }

	/* Run the service. */
      service_run();

      return 0;
}
