/*
   Author: James Beasley
   Repo: https://github.com/embeddedcognition/satclient
*/

#include <stdlib.h>              //using for "EXIT_..." macros
#include "lsm9ds0processor.h"    //using SAT processor logic

//global vars
static const int DESIRED_PROCESSING_LIMIT = 3000;   //stop the SAT process after it has sent the desired number of messages

//function declarations
int main(const int, const char**);

//function definition
//main thread of execution
int main(const int argc, const char** argv)
{
    //if the SAT process was successful
    if (perform_lsm9ds0_sat(DESIRED_PROCESSING_LIMIT))
    {
        //exit program, clean return code
        return EXIT_SUCCESS;
    }
    else
    {
        //exit program, with failure
        return EXIT_FAILURE;
    }
}
