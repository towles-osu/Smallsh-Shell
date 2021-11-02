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
#include <math.h>

//may need to add an include of <time.h>, but don't think I need it.

//------------Program Description--------------



//Defining structure for storing inputted commands
struct cmdStruct {
  char *name;
  int numOptions;
  char *options[512];
  int fgOrBg; //This should be 1 for foreground, 0 for backround
  int inRedirect; //This is 1 if there is an input redirect, otherwise 0
  int outRedirect; //this is 1 if there is an output redirect, otherwise 0
  char *inFilePath;
  char *outFilePath;
};


//--------myStrLen function------
//This is a custom function I made to calculate the length of a string
//Since we were warned against using strlen since it is not re-entrant
//it includes new line characters in the length of the string

int myStrLen(char *theStr){
  char curChar = theStr[0];
  int lenCount = 0;
  while (curChar != '\0'){
    lenCount++;
    curChar = theStr[lenCount];
  }
  return lenCount;
}


char *varExpand(char *word){
  //This function returns a copy of the word string with $$ replaced with pid
  char *resultStr = calloc(myStrLen(word) + 1, sizeof(char));
  strcpy(resultStr, word);
  char *tempStr;
  char *pidStr = calloc(15, sizeof(char));
  pid_t myPid = getpid();
  
  //manually converting int to string (couldn't get itoa to work)
  int count = 0;
  int digit;
  while (myPid != 0){
    digit = myPid % 10;
    switch (digit){
    case 0:
      pidStr[count] = '0';
      break;
    case 1:
      pidStr[count] = '1';
      break;
    case 2:
      pidStr[count] = '2';
      break;
    case 3:
      pidStr[count] = '3';
      break;
    case 4:
      pidStr[count] = '4';
      break;
    case 5:
      pidStr[count] = '5';
      break;
    case 6:
      pidStr[count] = '6';
      break;
    case 7:
      pidStr[count] = '7';
      break;
    case 8:
      pidStr[count] = '8';
      break;
    default:
      pidStr[count] = '9';
    }
    myPid = myPid - digit;
    myPid = myPid / 10;;
    count++;
  }

  //Now we invert pidStr since we wrote the digits in reverse order.
  int reverseCount = 0;
  char tempChar;
  while (count > reverseCount){
    tempChar = pidStr[count];
    pidStr[count] = pidStr[reverseCount];
    pidStr[reverseCount] = tempChar;
    reverseCount++;
    count = count -1;
  }


  //Here we loop while there are "$$" in the source string and replace them with the pid string
  int lenPidStr = myStrLen(pidStr);
  char *varIndex = strstr(resultStr, "$$");
  while (varIndex){
    tempStr = calloc(myStrLen(resultStr) + lenPidStr + 1, sizeof(char));
    for (int i = 0; i < (&varIndex - &tempStr); i++){
      tempStr[i] = resultStr[i];
    }
    strcat(tempStr, pidStr);
    strcat(tempStr, (varIndex + (2 * sizeof(char))));
    free(resultStr);
    resultStr = calloc(myStrLen(tempStr) + 1, sizeof(char));
    strcpy(resultStr, tempStr);
    free(tempStr);
    varIndex = strstr(resultStr, "$$");
  }

  free(pidStr);
  return resultStr;
}



//------readCommand function------
//This function is the top level function that gets called when we recieved a command

struct cmdStruct *readCommand(char *inStr){
  struct cmdStruct *parsedCmd = malloc(sizeof(struct cmdStruct));
  char *savePtr;
  char *curTok = strtok_r(inStr, " \n", &savePtr);
  int typeNextTok = 1;
  /* typeNextTok is how I tell the loop what it expects to see in the next token
     1 means the next token will be the command
     2 means we are expecting options
     3 means we encountered "<"
     4 means we encountered ">"
     5 means we encountered "&"
  */

  //Now we set default values of certain members of cmdStruct
  parsedCmd->fgOrBg = 1; //default to running in fg
  parsedCmd->numOptions = 0; //assumes no options to start with
  parsedCmd->inRedirect = 0; //assumed no input redirect until it encounters <
  parsedCmd->outRedirect = 0; //assumes no output redirect until it encounter >
  

  //Loop creating tokens for each space separated element of the input string
  while(curTok != NULL) {
    
    if (typeNextTok == 1) {
      //We are reading in the command.
      if (curTok[0] == '#'){
	break;//We just break out of the loop if comment and let comment catcher after loop deal with parsing
      } else {
	//Need to replace any double $ with pid
	curTok = varExpand(curTok);

	parsedCmd->name = calloc(myStrLen(curTok) + 1, sizeof(char));
	strcpy(parsedCmd->name, curTok);
	typeNextTok = 2;
      }
    } else if (typeNextTok == 2) {

      //Now we are reading options in
      if (strcmp(curTok, "<") == 0) {
	typeNextTok = 3;
      } else if (strcmp(curTok, ">") == 0){
	typeNextTok = 4;
      } else if (strcmp(curTok, "&") == 0){
	
	//this is the special case of when we hit an and-persand
	//we want to just immediately check the value of what will be next to see if we have hit the end of input
	curTok = strtok_r(NULL, " \n", &savePtr);
	if (curTok == NULL) {
	  parsedCmd -> fgOrBg = 0;
	  break;
	}
	else {
	
	  parsedCmd->options[parsedCmd->numOptions] = calloc(2, sizeof(char));
	  strcpy(parsedCmd->options[parsedCmd->numOptions], "&");
	  parsedCmd->numOptions++;
	  continue;
	}
      } else {
	//If we get here we have recieved a normal input option, just need to replace
	// double $ with pid and save it into the options list
	
       	//parse curTok for $$ and replace any $$ with pid
	curTok = varExpand(curTok);
	
	//append result string to options list
	parsedCmd->options[parsedCmd->numOptions] = calloc(myStrLen(curTok)+1, sizeof(char));
	strcpy(parsedCmd->options[parsedCmd->numOptions], curTok);
	parsedCmd->numOptions++;
      }
    }
    
    curTok = strtok_r(NULL, " \n", &savePtr);
  }

  //if typeNextTok never changed from 1 then we recieved a blank line, set name to "#"
  if (typeNextTok == 1) {
    strcpy(parsedCmd->name, "#");//This means line was blank or a comment
  }

  return parsedCmd;
}


  /* Decided it is better to do this with tokens
 char curChar inStr[0];
  struct cmdStruct parsedCmd;
  int parseCount = 0;
  int parsePhase = -1;
  /* parsePhase is used to determine what piece of a command we are parsing
     -1 means we haven't started parsing, and allows us to ignore preceding spaces to command
     0 means we don't know what we are parsing because we hit a space
     1 means we are parsing the command
     2 means we are parsing an option
 


  if (curChar == '\n' || curChar == '#'){
    parsedCmd.name = "#";
    parsedCmd.numOptions = 0;
    return parsedCmd;
  }

  parsedCmd.name = "";

  while (curChar != '\n'){
    //First we deal with the command, which should be the first word in the input
    if (parsePhase == -1) {
      if (curChar == ' '){
	parseCount++;
	curChar = inStr[parseCount];
	continue;
      }
      else parsePhase = 1;
    }

    if (parsePhase == 1) {
      
    }

    parseCount++;
    curChar = inStr[parseCount];
  }
  //iterate through the characters of the command

}


/*This version kinda gets to some of the functionality, now re-writing for cmdStruct
int readCommand(char *inStr){
 
  //setup forking variabls
  int childExitInt;
  pid_t splittingPid = fork();

  //Deal with possible fork cases, in parent wait, in child process input
  if (splittingPid == -1){
    perror("fork() failed\n");
    exit(1);

  } else if (splittingPid == 0) {
    //We are in the child process now, where we will read the command
    write(STDOUT_FILENO, "your command was: ", 19);
    fflush(stdout);
    write(STDOUT_FILENO, inStr, sizeof(&inStr) / sizeof(char));
    fflush(stdout);
    write(STDOUT_FILENO, "\n", 2);
    fflush(stdout);
    exit(0);
  } else {  
    waitpid(splittingPid, &childExitInt, 0);
    printf("DONE WITH EXIT CODE %d", childExitInt);
    fflush(stdout);
    return 0;
  }
  return childExitInt;
}

/* Starting over on implementing this
int readCommand(char **cmndStr){
  char outstr[3000];
  strcpy(outstr, cmndStr);
  strcat(outstr, " was your command");
  write(STDOUT_FILENO, outstr, strlen(outstr));
}
*/







//------main function------


int main(int argc, char *argv[]){
  
  int keepRunning = 1;
  //char *inputString = calloc(2049, sizeof(char));
  char *inputString;
  int forkedId;
  struct cmdStruct *curCmd = malloc(sizeof(struct cmdStruct));
  


  //Main loop, will ask for a prompt by writing : and waiting for input
  while (keepRunning == 1) {
    //allocate memory for the input
    inputString = calloc(2049, sizeof(char));//2049 to allow input stirngs of up to 2048 chars

    write(STDOUT_FILENO, ":", 1);
    fflush(stdout);
    
    //NEED TO CHANGE THIS TO fgets I think.
    //read(STDIN_FILENO, &inputString, 2049 * sizeof(char));
    fgets(inputString, 2049, stdin); 
    curCmd = readCommand(inputString);
    
    //just some testing code print statements
    char *testingStr = calloc(myStrLen(curCmd->name) + 18, sizeof(char));
    strcpy(testingStr, curCmd->name);
    strcat(testingStr, " was the command\n");
    write(STDOUT_FILENO, testingStr, myStrLen(testingStr) + 1);

    //Handling built-in commands
    if (strcmp(curCmd->name, "exit")==0){
      keepRunning = 0;
    } else if (strcmp(curCmd->name, "cd") == 0){
      //cd command
    }

    //Handling any input or output redirection
    //This is done in readCommand function, and values in our curCmd struct tell us if we need to do this

    //Handling commands beyond the built-in ones
    //WRITE THIS

    //Handling foreground or background commands
    //WRITE THIS
    //Thoughts: Since we know if a command will be foreground or background after parsing curCmd
    //  we can make this part of handling commands beyond the built-in ones since
    // it is another forking condition?

    //Signal interupt handling
    //WRITE THIS

    //de-allocate memory for the input
    free(inputString);
    //de-allocate memory for the command
    //WRITE THIS
}

  return EXIT_SUCCESS;

}
