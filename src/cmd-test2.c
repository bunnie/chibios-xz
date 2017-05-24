#include "hal.h"
#include "shell.h"
#include "chprintf.h"


void test2Command(BaseSequentialStream *chp, int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  
  chprintf(chp, "hello world"SHELL_NEWLINE_STR);

  return;
}
