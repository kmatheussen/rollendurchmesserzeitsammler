
#include <stdio.h>

void GC_register_dynamic_libraries(void);

static char *glob_source;
static char *glob_start;
static char *glob_end;

void GC_add_roots_inner(char *start,char *end,int abool){
  //fprintf(stderr,"GC_add_roots_inner called. start: %p, end: %p, abool: %d, size: %d\n",start,end,abool,end-start);
  if(glob_source>=start && glob_source<end){
    glob_start=start;
    glob_end=end;
  }
}

char *GC_data_start;

// Returns 0 in case it wasn't found.
// Not thread safe.
int tar_get_dynamic_roots_for(char *pointer,char **start,char **end){
  glob_source=pointer;
  glob_start=NULL;
  glob_end=NULL;
  GC_register_dynamic_libraries();
  *start=glob_start;
  *end=glob_end;
  return *start==NULL?0:1;
}

