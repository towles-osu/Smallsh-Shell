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


//Global variables
//Cur status will always be an int that is the status of the most recent exit code of any process
int curStatus = 0;


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


//-------intToStr function-------
//This function recieves an integer and returns a pointer to a string on the heap that is the int as a string
char *intToStr(int num){
  //Often this will be used to convert a 0 to a string, so here is special case for just that
  if (num == 0){
    char* zeroRet = calloc(2, sizeof(char));
    strcpy(zeroRet, "0");
    return zeroRet;
  }


  //We call our string of the int pidStr because we copied this code from our varExpand method
  char pidStr[25] = "";
  int myPid = num; //using this var name so I can use my code copy and pasted from the varExpand method
  int count = 0;
  int digit;

  //deal with scenario where number is negative
  if (num < 0) {
    pidStr[0] = '-';
    count = 1;
    myPid = myPid * -1;
  }

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

  count = count -1;
  //Now we invert pidStr since we wrote the digits in reverse order.
  int reverseCount = 0;
  //deal with case where num was negative so we don't reverse the first character
  if (num < 0) {
    reverseCount = 1;
  }
  char tempChar;
  while (count > reverseCount){
    tempChar = pidStr[count];
    pidStr[count] = pidStr[reverseCount];
    pidStr[reverseCount] = tempChar;
    reverseCount++;
    count = count -1;
  }
  char *result = calloc(myStrLen(pidStr) + 1, sizeof(char));
  strcpy(result, pidStr);
  return result;
}


char *varExpand(char *word){
  //This function returns a copy of the word string with $$ replaced with pid
  char *resultStr = calloc(myStrLen(word) + 1, sizeof(char));
  strcpy(resultStr, word);
  char *tempStr;
  char pidStr[15] = ""; 
  pid_t myPid = getpid();


  //manually converting int to string (couldn't get itoa to work)
  //Later I wrote a helper function to do this, but to avoid having additional bugs come up
  //I left this hand written inline version in this function
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

  count = count -1;
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
    for (int i = 0; i < (varIndex - resultStr); i++){
      tempStr[i] = resultStr[i];
    }//This for copies over the string up to the $$
    strcat(tempStr, pidStr);
    strcat(tempStr, (varIndex + (2 * sizeof(char))));//this copies the rest of string after $$
    free(resultStr);
    resultStr = calloc(myStrLen(tempStr) + 1, sizeof(char));
    strcpy(resultStr, tempStr);
    free(tempStr);
    varIndex = strstr(resultStr, "$$");
  }


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

	//Also, make first option the name of the command for ease of exec functions
	parsedCmd->options[parsedCmd->numOptions] = calloc (myStrLen(curTok) + 1, sizeof(char));
	strcpy(parsedCmd->options[parsedCmd->numOptions], curTok);
	parsedCmd->numOptions++;
      }
    } else if (typeNextTok == 2) {

      //Now we are reading options in
      if (strcmp(curTok, "<") == 0 && (strcmp(parsedCmd->name, "exit") != 0 && strcmp(parsedCmd->name, "cd") != 0 
				       && strcmp(parsedCmd->name, "status"))) {
	parsedCmd->inRedirect = 1;
	typeNextTok = 3;
      } else if (strcmp(curTok, ">") == 0 && (strcmp(parsedCmd->name, "exit") != 0 && strcmp(parsedCmd->name, "cd") != 0 
					      && strcmp(parsedCmd->name, "status"))){
	typeNextTok = 4;
	parsedCmd->outRedirect = 1;
      } else if (strcmp(curTok, "&") == 0){
	
	//this is the special case of when we hit an and-persand
	//we want to just immediately check the value of what will be next to see if we have hit the end of input
	curTok = strtok_r(NULL, " \n", &savePtr);
	if (curTok == NULL && (strcmp(parsedCmd->name, "exit") != 0 && strcmp(parsedCmd->name, "cd") != 0 
			       && strcmp(parsedCmd->name, "status"))){
	  parsedCmd -> fgOrBg = 0;
	  break;
	}
	else {
	
	  //NEED TO WRITE conditional to determine if & is filepath for a redirect
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
    parsedCmd->name = calloc(2, sizeof(char));
    strcpy(parsedCmd->name, "#");//This means line was blank or a comment
  }

  return parsedCmd;
}






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
      //NEED TO ADD kill all child processes
    } else if (strcmp(curCmd->name, "cd") == 0){
      //cd command
      char *curCwd = malloc(PATH_MAX);
      getcwd(curCwd, PATH_MAX);
     
      if (curCmd->numOptions == 1) {
	//if no options then we change the working directory to the home directory
	chdir(getenv("HOME"));


      } else {
	//If we have options, we change to the first option as our cwd

	//first, determine if the new path is absolute or relative
	if (curCmd->options[1][0] == '/'){
	  //path is absolute
	  chdir(curCmd->options[1]);
	  
	} else {
	  //path is relative
	  char *tempPath = calloc(myStrLen(curCwd) + myStrLen(curCmd->options[1]) + 2, sizeof(char));
	  strcpy(tempPath, curCwd);
	  strcat(tempPath, "/");
	  strcat(tempPath, curCmd->options[1]);
	  chdir(tempPath);
	  free(tempPath);
	}
      }
      free(curCwd);
    } else if (strcmp(curCmd->name, "status") == 0) {
      
      //status command has been called
      write(STDOUT_FILENO, "exit value ", 12);
      char* strStat = intToStr(curStatus);
      write(STDOUT_FILENO, strStat, myStrLen(strStat) + 1);
      write(STDOUT_FILENO, "\n", 2);
      fflush(stdout);
      free(strStat);

    } else {
      
      //We have gotten a command that must be run with exec funciton.
      
      //inside here we will have two different ways of dealing with things based on if it is
      //fg or bg

      //also we will do any needed redirecting of stdout and stdin before calling the exec function

      //then we will fork and call the exec

      if (curCmd->fgOrBg == 1) {
	
	//The curCmd should be run in the foreground
	//First, we setup where input and output should be read from based on if we have redirects
	if (curCmd->inRedirect == 0) {
	  //we are recieving redirected input
	}
	
	//Initialize forking variables
	int childStatus;
	pid_t spawnPid = fork();
	if (spawnPid == -1) {
	  //failed fork, write error exit with 1
	  perror("fork() failed");
	  exit(1);
	} else if (spawnPid == 0) {
	  
	  //We are in child process

	  execvp(curCmd->options[0], curCmd->options);
	  perror("execv failed");
	  exit(1);
	  break;
	  
	} else {
	  //we are in the parent process
	  spawnPid = waitpid(spawnPid, &childStatus, 0);
	  curStatus = childStatus;
	  printf("parent done");
	  fflush(stdout);
	  
	}
	
      } else {
	
	//the curCmd should be run in the background
      }

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
    //May need to close up memory leaks in curCmd for the attributes
    free(curCmd->name);
    free(curCmd);
  }

  return EXIT_SUCCESS;

}
