

#include "DataModel.h"

// FLTK includes
#include <FL/fl_ask.H>

// C libraries

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

int DataModel::LoadData(const char * filename)
{
  printf ("LoadData %s\n", filename);

  char *suffix = strrchr(filename, '-');
  if (!suffix) {
    fprintf (stderr,
	     "LoadData: file %s does not appear to be generated by Chapel\n",
	     filename);
    return 0;
  }
  suffix += 1;
  int namesize = strlen(filename) - strlen(suffix);
  int fileno = atoi(suffix);

  printf ("LoadData:  namesize is %d, fileno is %d\n", namesize, fileno);
  
  FILE *data = fopen(filename, "r");
  if (!data) {
    fprintf (stderr, "LoadData: Could not open %s.\n", filename);
    return 0;
  }

  // Read the config data
  char configline[100];

  if (fgets(configline, 100, data) != configline) {
    fprintf (stderr, "LoadData: Could not read file %s.\n", filename);
    fclose(data);
    return 0;
  }

  // The configuration data
  int nlocales;
  int fnum;
  double seq;

  if (sscanf(configline, "ChplVdebug: nodes %d, id %d, seq %lf",
	     &nlocales, &fnum, &seq) != 3) {
    fprintf (stderr, "LoadData: incorrect data on first line of %s.\n",
	     filename);
    fclose(data);
    return 0;
  }
  fclose(data);

  char fname[namesize+15];
  printf ("LoadData: nlocalse = %d, fnum = %d seq = %.3lf\n", nlocales, fnum, seq);

  // Set the number of locales.
  numLocales = nlocales;

  for (int i = 0; i < nlocales; i++) {
    snprintf (fname, namesize+15, "%.*s%d", namesize, filename, i);
    if (!LoadFile(fname, i, seq)) {
      fprintf (stderr, "Error processing data from %s\n", fname);
      numLocales = -1;
      return 0;
    }
  }


  return 1;
}


// Load the data in the current file

int DataModel::LoadFile (const char *filename, int index, double seq)
{
  FILE *data = fopen(filename, "r");
  char line[1024];

  int floc;
  int findex;
  double fseq;

  if (!data) return 0;

  printf ("LoadFile %s\n", filename);
  if (fgets(line,1024,data) != line) {
    fprintf (stderr, "Error reading file %s.\n", filename);
    return 0;
  }

  // First line is the information line ... 

  if (sscanf(line, "ChplVdebug: nodes %d, id %d, seq %lf",
	     &floc, &findex, &fseq) != 3) {
    fprintf (stderr, "LoadData: incorrect data on first line of %s.\n",
	     filename);
    fclose(data);
    return 0;
  }

  // Verify the data

  if (floc != numLocales || findex != index || fabs(seq-fseq) > .01 ) {
    printf ("Mismatch %d = %d, %d = %d, %.3lf = %.3lf\n", floc, numLocales, findex,
	    index, fseq, seq);
    fprintf (stderr, "Data file %s does not match selected file.\n", filename);
    return 0;
  }

  // Now read the rest of the file

  while ( fgets(line, 1024, data) == line ) {
    char *linedata;
    long sec;
    long usec;

    // Process the line
    linedata = strchr(line, ':');
    if (linedata) {
      if (sscanf (linedata, ": %ld.%ld", &sec, &usec) != 2) {
	printf ("Can't read time from '%s'\n", linedata);
      }
    }

    // printf("%c", line[0]);
    switch (line[0]) {

      case 0:  // Bug in output???
	break;

      case 't':  // new task line
	break;

      case 'e':  // end task (not generated yet)
        printf ("E");
	break;

      case 'n':  // non-blocking put or get
      case 's':  // strid put or get
      case 'g':  // regular get
      case 'p':  // regular put
	// All similar data ... 
	break;
      
      default:
        printf ("X");
        //fl_alert("Bad input data.");
    }
  }
  printf("\n");

  if ( !feof(data) ) return 0;

  return 1;
}
