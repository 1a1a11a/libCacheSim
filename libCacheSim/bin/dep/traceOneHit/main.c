
#include <libgen.h>

#include "internal.h"

int main(int argc, char *argv[]) {
  struct arguments args;
  parse_cmd(argc, argv, &args);

  if (strcasecmp(args.task, "calOneHit") == 0) {
    if (args.ofilepath[0] == '\0') {
      snprintf(args.ofilepath, OFILEPATH_LEN, "%s.oneHit",
               basename(args.trace_path));
    }

    cal_one_hit(args.reader, args.ofilepath);
  } else {
    fprintf(stderr, "Unknown task: %s\n", args.task);
  }

  close_reader(args.reader);

  return 0;
}
