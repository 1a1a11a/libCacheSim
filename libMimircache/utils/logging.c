
#include "../../include/mimircache/logging.h"


#ifdef __cplusplus
extern "C"
{
#endif



int log_header(int level, const char *file, int line)
{
    if(level < MIMIR_LOGLEVEL) {
        return 0;
    }

    switch(level) {
        case DEBUG3_LEVEL:
            printf("%s[DEBUG3] ", BLUE);
            break;
        case DEBUG2_LEVEL:
            printf("%s[DEBUG2] ", BLUE);
            break;
        case DEBUG_LEVEL:
            printf("%s[DEBUG]   ", BLUE);
            break;
        case INFO_LEVEL:
            printf("%s[INFO]    ", GREEN);
            break;
        case WARNING_LEVEL:
            printf("%s[WARNING] ", YELLOW);
            break;
        case SEVERE_LEVEL:
            printf("%s[ERROR] ", RED);
            break;
      default:
            printf("in logging should not be here\n");
            break;
    }

    char buffer[30];
    struct timeval tv;
    time_t curtime;

    gettimeofday(&tv, NULL);
    curtime = tv.tv_sec;
    strftime(buffer, 30, "%m-%d-%Y %T", localtime(&curtime));

    printf("%s %16s:%-4d ", buffer, strrchr(file, '/')+1, line);
    printf("(tid=%zu): ", (unsigned long) pthread_self());

    return 1;
}


void print_stack_trace(void){
    
    void *array[10];
    size_t size;
    
    // get void*'s for all entries on the stack
    size = backtrace(array, 10);
    
    // print out all the frames to stderr
    fprintf(stderr, "stack trace: \n");
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    exit(1);
}


#ifdef __cplusplus
}
#endif
