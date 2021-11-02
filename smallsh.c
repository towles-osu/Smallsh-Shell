//Author: Stew Towle
//Written for OSU's CS344 Operating Systems class, Assignment 3
//Must be compiled with gcc compiler and --std=gnu99 option

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>

//may need to add an include of <time.h>, but don't think I need it.

//------------Program Description--------------




//------readCommand function------
//This function is the top level function that gets called when we recieved a command

int readCommand(char **cmndStr){
  char outstr[3000];
  strcpy(outstr, cmndStr);
  strcat(outstr, " was your command");
  write(STDOUT_FILENO, outstr, strlen(outstr));
}








//------main function------


int main(int argc, char *argv[]){
  
  int keepRunning = 1;
  char *inputString = calloc(2049, sizeof(char));


  //Main loop, will ask for a prompt by writing : and waiting for input
  while (keepRunning == 1) {
    write(STDOUT_FILENO, ":", 1);
    
    //NEED TO CHANGE THIS TO fgets I think.
    read(STDIN_FILENO, &inputString, 2049 * sizeof(char));
    readCommand( &inputString);
  }

  return EXIT_SUCCESS;

}
