//
// Created by Juncheng Yang on 5/14/20.
//

#ifndef LIBMIMIRCACHE_RESOURCEMEASURE_H
#define LIBMIMIRCACHE_RESOURCEMEASURE_H


#include <stdio.h>
#include <sys/time.h>
#include <sys/resource.h>


#ifdef __cplusplus
extern "C"
{
#endif

#define TIMEVAL_TO_USEC(tv) ((long long) (tv.tv_sec*1000000+tv.tv_usec))
#define TIMEVAL_TO_SEC(tv) ((double) (tv.tv_sec+tv.tv_usec/1000000.0))

int get_resouce_usage() {
  struct rusage r_usage;
  getrusage(RUSAGE_SELF, &r_usage);



  return 0;
}

int print_resource_usage(){
  struct rusage r_usage;
  getrusage(RUSAGE_SELF, &r_usage);

  printf("CPU user time %lld us, sys time %lld us\n", TIMEVAL_TO_USEC(r_usage.ru_utime), TIMEVAL_TO_USEC(r_usage.ru_stime));
  printf("Mem RSS %.2lf MB, soft page fault %ld - hard page fault %ld, block input %ld - block output %ld, voluntary context switches %ld - involuntary %ld\n",
         (double) r_usage.ru_maxrss/(1024.0), r_usage.ru_minflt, r_usage.ru_majflt, r_usage.ru_inblock, r_usage.ru_oublock, r_usage.ru_nvcsw, r_usage.ru_nivcsw);
  return 0;
}

void print_rusage_diff0(struct rusage r1, struct rusage r2){
  printf("******  CPU user time %.2lf s, sys time %.2lf s\n",
      (TIMEVAL_TO_SEC(r2.ru_utime)-TIMEVAL_TO_SEC(r1.ru_utime)),
      (TIMEVAL_TO_SEC(r2.ru_stime)-TIMEVAL_TO_SEC(r1.ru_stime)));

  printf("******  Mem RSS %.2lf MB, soft page fault %ld - hard page fault %ld, block input %ld - block output %ld, voluntary context switches %ld - involuntary %ld\n",
         (double) (r2.ru_maxrss-r1.ru_maxrss)/(1024.0),
         (r2.ru_minflt-r1.ru_minflt), (r2.ru_majflt-r1.ru_majflt),
         (r2.ru_inblock-r1.ru_inblock), (r2.ru_oublock-r1.ru_oublock),
         (r2.ru_nvcsw-r1.ru_nvcsw), (r2.ru_nivcsw-r1.ru_nivcsw));
}


void print_rusage_diff(struct rusage r1, struct rusage r2){
  printf("******  CPU user time %.2lf s, sys time %.2lf s\n",
         (TIMEVAL_TO_SEC(r2.ru_utime)-TIMEVAL_TO_SEC(r1.ru_utime)),
         (TIMEVAL_TO_SEC(r2.ru_stime)-TIMEVAL_TO_SEC(r1.ru_stime)));

  printf("******  Mem RSS %.2lf MB, soft page fault %ld - hard page fault %ld, voluntary context switches %ld - involuntary %ld\n",
         (double) (r2.ru_maxrss-r1.ru_maxrss)/(1024.0),
         (r2.ru_minflt-r1.ru_minflt), (r2.ru_majflt-r1.ru_majflt),
         (r2.ru_nvcsw-r1.ru_nvcsw), (r2.ru_nivcsw-r1.ru_nivcsw));
}


#ifdef __cplusplus
}
#endif


#endif //LIBMIMIRCACHE_RESOURCEMEASURE_H
