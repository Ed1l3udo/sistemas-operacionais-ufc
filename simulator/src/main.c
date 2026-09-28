#include <stdio.h>

#include "app.h"

int main(int argc, char *argv[])
{
    return scheduler_run_application(argc, argv, stdin, stdout, stderr);
}
