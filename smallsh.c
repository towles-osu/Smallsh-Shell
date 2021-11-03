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
#include <signal.h>
//#include <math.h>

//may need to add an include of <time.h>, but don't think I need it.

//------------Program Description--------------


//Global variables
//Cur status will always be an int that is the status of the most recent exit code of any process
int curStatus = 0;
int fgOnlyMode = 0;//This becomes 1 if we are in fg only mode, is 0 if not.


//Defining structure for storing inputted commands
struct cmdStruct {
  char *name;
  int numOptions;
  char *options[512];//option list, the first of which will be the command itself, and thus identitical to name
  int fgOrBg; //This should be 1 for foreground, 0 for backround
  int inRedirect; //This is 1 if there is an input redirect, otherwise 0
  int outRedirect; //this is 1 if there is an output redirect, otherwise 0
  char *inFilePath;
  char *outFilePath;
  int validInput;//This should be 1 if the command is formatted correctly, and becomes 0 in certain
                 // invalidly formatted input scenarios
};

//Defining Linked List Node structure for storing bg processes
struct bgProcessNode {
    pid_t processId; //if this is -1 it is the head node, otherwise will be ID of a bg process
    struct bgProcessNode nextNode;
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


//-------varExpand function-------
//Description: This function take a string that should not have spaces in it (already split up by readCommand)
//            and returns  a new string (which is on the heap) that has replaced any $$ with the PID

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
    strcat(tempStr, pidStr);//This appends the pid
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
//Its purpose is to create a cmdStruct that has all the info we need for determining how to handle the command

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
     6 means we have taken an input redirect and will only accept an output redirect or "&"
     7 means we have taken an output redirect and will only accept a "&"
  */

  //Now we set default values of certain members of cmdStruct
  parsedCmd->fgOrBg = 1; //default to running in fg
  parsedCmd->numOptions = 0; //assumes no options to start with
  parsedCmd->inRedirect = 0; //assumed no input redirect until it encounters <
  parsedCmd->outRedirect = 0; //assumes no output redirect until it encounter >
  parsedCmd->validInput = 1;//Assumes the input will be valid, this is set to 0 if we parsed the input but find it invalid

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
	
	  //If the & is not at the end of the input then we get here, and treat it as a regular input argument
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
    } else if (typeNextTok == 3) {
      //I am doing a varExpand on the token here, though I am a little unsure if that is what I should do
      //based on the assignment description.  However, I chose to do it, in case someone does something like:
      //make a file using $$ to give it a name with the PID of the parent process in it, and then later
      //wants to redirect that files contents as the input for another command and wants to be able to 
      //reference that file with the $$ to autofill as the PID.
      curTok = varExpand(curTok);

      //we are on a token that immediately follows a "<", so we want to store it as the inFilePath
      parsedCmd->inFilePath = calloc(myStrLen(curTok) + 1, sizeof(char));//designate memory for file path
      strcpy(parsedCmd->inFilePath, curTok); //copy over filepath
      typeNextTok = 6;//Set to 6 designating that now we only accept output redirects or background trigger
    } else if (typeNextTok == 4) {
      //In this if scenario we have encountered the output redirect character
      //First, for similar reason to in the if directly before this one, we varExpand any $$
      curTok = varExpand(curTok);

      //Then we copy the filePath into the outFilePath attribute
      parsedCmd->outFilePath = calloc(myStrLen(curTok) + 1, sizeof(char));
      strcpy(parsedCmd->outFilePath, curTok);
      typeNextTok = 7;//Indicates only acceptable additional input is the "&" to designate bg process
    } else if (typeNextTok == 6) {
    //SPECIAL NOTE: I skip typeNextTok == 5 because it is not an actual potential state, since if we hit 
    // a "&" before getting any in/out redirect symbols we will deal with it appropriately
    //  in the if statement for typeNextTok ==2, otherwise, we will be arriving at it when typeNextTok
    //  is either 6 or 7 (since it will be after an input or output redirect)
    
      //Now we are in the scenario where we recieved an input redirection and already stored the filepath
      //We want to check if there is output redirection or a run in background symbol
      if (strcmp(curTok, ">") == 0) {
	//Here we set boolean for output redirection and set typeNextTok to 4 to indicate we will read filepath
	parsedCmd->outRedirect = 1;
	typeNextTok = 4;
      } else if (strcmp(curTok, "&") == 0) {
	//this is the special case of when we hit an and-persand
	//we want to just immediately check the value of what will be next to see if we have hit the end of input
	curTok = strtok_r(NULL, " \n", &savePtr);
	if (curTok == NULL){
	  //If the & is the final character we set command to run in background
	  parsedCmd -> fgOrBg = 0;
	  break;
	}
	//if we hit an & but there are more commands after it, the user has entered improper input
	//I could not find a clear indication of how the assignment wants us to handle this
	//So I have chosen to use a field in cmdStruct to designate a general state of
	//improrly formatted input and break from the command parsing loop
	parsedCmd-> validInput = 0;
	break;
      }

    } else if (typeNextTok == 7) {
      
      //Here we have had a output redirect and dealt with it, so the only valid value for the token
      //now would be a "&" command to indicate run in bg
      if(strcmp(curTok, "&") == 0) {
	
	//check that this is the final character by getting next token
	curTok = strtok_r(NULL, " \n", &savePtr);
	if(curTok == NULL) {
	  parsedCmd-> fgOrBg = 0;
	  break;
	}
      }
      //if we get here then there are invalid input fields at the end of our input string
      //So I indicate this with the validInput boolean in the parseCmd
      parsedCmd-> validInput = 0;
      break;
    }


    curTok = strtok_r(NULL, " \n", &savePtr);
  }

  //if typeNextTok never changed from 1 then we recieved a blank line, set name to "#"
  if (typeNextTok == 1) {
    parsedCmd->name = calloc(2, sizeof(char));
    strcpy(parsedCmd->name, "#");//This means line was blank or a comment
  }

  //Deal with a scenario where we recieved a "<" input redirect symbol but nothing after it
  if (typeNextTok == 3) {
    //Currently we do nothing in this scenario except make the file name explicitly NULL
    //This is so that if we set input redirect to true for this command we will be able to identify
    //that no filepath was given when attempting to run the command with a redirect
    parsedCmd->inFilePath = NULL;
  }
  
  //Similarly, deal with scenario where we recieved a ">" without a filepath after
  if (typeNextTok == 4) {
    parsedCmd -> outFilePath = NULL;
  }

  return parsedCmd;
}






//------main function------


int main(int argc, char *argv[]){
  
  //Define variable that will be used throughoutout
  int keepRunning = 1;
  //char *inputString = calloc(2049, sizeof(char));
  char *inputString;
  int forkedId;
  struct cmdStruct *curCmd = malloc(sizeof(struct cmdStruct));
  sigset_t signalSet;
  sigemptyset(&signalSet);
  if (sigaddset(&signalSet, SIGINT) == -1){
    perror("failed to add SIGINT");
  }
  if (sigaddset(&signalSet, SIGTSTP) == -1){
    perror("failed to add SIGTSTP");
  }
  struct bgProcessNode *headNode = malloc(sizeof(struct bgProcessNode));
  headNode->processId = -1;
  headNode->nextNode = NULL;
  struct bgProcessNode *curNode = malloc(sizeof(struct bgProcessNode));
  struct bgProcessNode *prevNode = malloc(sizeof(struct bgProcessNode));
  int bgChildStatus;
  pid_t bgChildPid;

  //NEED TO INITIALIZE SIGNAL HANDLERS


  //Main loop, will ask for a prompt by writing : and waiting for input
  while (keepRunning == 1) {
    //allocate memory for the input
    inputString = calloc(2049, sizeof(char));//2049 to allow input stirngs of up to 2048 chars

    //Reset curNode and iterate through nodes checking if bgProcess completed
    curNode = headNode;
    //bgChildStatus = 0;
    while(curNode->nextNode){
    //If there is a next node then check if that process is complete
      bgChildStatus = NULL;
	  prevNode = curNode;
      curNode = curNode->nextNode;
      bgChildPid = waitpid(curNode->processId, &bgChildStatus, WNOHANG);
      WIFEXITED(bgChildStatus){
        write(STDOUT_FILENO, "background pid ", 16);
        char *pidStrTemp = intToStr(bgChildPid);
        write(STDOUT_FILENO, pidStrTemp, myStrLen(pidStrTemp) + 1);
        write(STDOUT_FILENO, " is done:\n", 11);
        fflush(stdout);
	    free(pidStrTemp);
		prevNode->nextNode = curNode->nextNode;
		free(curNode);
        //NEED TO INCLUDE EXIT VALUE OR TERMINATION SIGNAL AS WELL
      }
    }


    write(STDOUT_FILENO, ":", 1);
    fflush(stdout);
    
    //NEED TO CHANGE THIS TO fgets I think.
    //read(STDIN_FILENO, &inputString, 2049 * sizeof(char));
    fgets(inputString, 2049, stdin); 
    curCmd = readCommand(inputString);
    
    /* //just some testing code print statements
    char *testingStr = calloc(myStrLen(curCmd->name) + 18, sizeof(char));
    strcpy(testingStr, curCmd->name);
    strcat(testingStr, " was the command\n");
    write(STDOUT_FILENO, testingStr, myStrLen(testingStr) + 1);
    */

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

      if (curCmd->fgOrBg == 1 || fgOnlyMode == 1) {
	
	//The curCmd should be run in the foreground
	//This means we are either in fg only mode or the command was not requested to be run in bg


	//Initialize forking variables
	int childStatus;//will use this to track status of child 
	pid_t spawnPid = fork();


	if (spawnPid == -1) {
	  //failed fork, write error exit with 1
	  perror("fork() failed");
	  exit(1);
	} else if (spawnPid == 0) {
	  
	  //We are in child process
	  
	  //We want to check if we need to do any input or output redirection
	  //We want to do so before calling the command so that our filestreams are setup when the command runs
	  //We also need to declare our file variable out here so we have them outside the conditionals

	  int sourceFD;
	  int dupResult;
	  int targetFD;
	  int dupOutRes;
	  if (curCmd->inRedirect == 1) {
	    //We have a command that is attempting to redirect input from a file
	    
	    //Attempt to open the file, if it doesn't work we need to print and error message and update status
	    //Though status updating will be handled by the parent process based on our exit code
	    //since we are running in the foreground we don't need to have the filestream available to parent and child
	    if (curCmd->inFilePath) {//check that there is actually a filePath to attempt to read from
	      printf(curCmd -> inFilePath);
	      fflush(stdout);
	      //Now we attempt to read from the file path
	      sourceFD = open(curCmd->inFilePath, O_RDONLY);
	      if (sourceFD == -1) {
		perror("cant read from file");
		exit(1);
	      }
	      dupResult = dup2(sourceFD, 0);//Set the file as standard in's source
	      if (dupResult == -1) {
		//if this fails then print error and exit with 1
		perror("dup2 error");
		exit(1);
	      }
	    } else {
	      //if there was no filepath then print error message and send exit status of 1
	      perror("No filepath provided for input redirect");
	      exit(1);
	    }
	    //At this point I belive we have succesfully redirected stdin to be from the file
	  }

	  if (curCmd->outRedirect == 1){
	    //we have a command that is attempting to redirect output to file

	    //process mirrors above process, start with checking file path exists
	    if (curCmd->outFilePath) {
	      
	      //Now attempt to open file
	      targetFD = open(curCmd->outFilePath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	      if (targetFD == -1) {
		perror("cant write to file");
		exit(1);
	      }
	      dupOutRes = dup2(targetFD, 1);
	      if (dupOutRes == -1){
		//error with dup2 on write
		perror("dup2 error output redirect");
		exit(1);
	      }
	    } else {
	      //no filepath provided so error
	      perror("No filepath provided for output redirect");
	      exit(1);
	    }
	    //At this point we should have successfully redirected standard out
	  }
	  
	  if (curCmd->inRedirect == 1) {
	    //if we have input redirect we dont want to use options to determine inputs
	    execlp(curCmd->name, curCmd->name, NULL);//this should use our stdin stdout as set
	  }
	  else {
	    //if we didnt have input redirect, options come from command struct
	    execvp(curCmd->options[0], curCmd->options);
	  }
	  perror("exec func failed");//writing error message
	  exit(1);
	  break;
	  
	} else {
	  //we are in the parent process
	  spawnPid = waitpid(spawnPid, &childStatus, 0);
	  if (childStatus != 0){
	    curStatus = 1;
	  }
	  else {
	    curStatus = 0;
	  }
	  //printf("parent done");
	  //fflush(stdout);
	  
	}
	
      } else {
	
	//the curCmd should be run in the background
        //Initialize forking variables
	int childStatus;//will use this to track status of child 
	pid_t spawnPid = fork();


	if (spawnPid == -1) {
	  //failed fork, write error exit with 1
	  perror("fork() failed");
	  exit(1);
	} else if (spawnPid == 0) {
	  
	  //We are in child process

	  //First we will print the id of the background process
	  write(STDOUT_FILENO, "background pid is ", 19);
	  char *tempPidStr = intToStr(getpid());
	  write(STDOUT_FILENO, tempPidStr, myStrLen(tempPidStr) + 1);
	  free(tempPidStr);
	  write(STDOUT_FILENO, "\n", 2);
	  fflush(stdout);

	
	  //We want to check if we need to do any input or output redirection
	  //We want to do so before calling the command so that our filestreams are setup when the command runs
	  //We also need to declare our file variable out here so we have them outside the conditionals

	  int sourceFD;
	  int dupResult;
	  int targetFD;
	  int dupOutRes;
	  //NOTE THIS: to disable redirects for testing purposes have added an impossible condition here
	  if (curCmd->inRedirect == 1 && 1 == 2) {
	    //We have a command that is attempting to redirect input from a file
	    
	    //Attempt to open the file, if it doesn't work we need to print and error message and update status
	    //Though status updating will be handled by the parent process based on our exit code
	    //since we are running in the foreground we don't need to have the filestream available to parent and child
	    if (curCmd->inFilePath) {//check that there is actually a filePath to attempt to read from
	      printf(curCmd -> inFilePath);
	      fflush(stdout);
	      //Now we attempt to read from the file path
	      sourceFD = open(curCmd->inFilePath, O_RDONLY);
	      if (sourceFD == -1) {
		perror("cant read from file");
		exit(1);
	      }
	      dupResult = dup2(sourceFD, 0);//Set the file as standard in's source
	      if (dupResult == -1) {
		//if this fails then print error and exit with 1
		perror("dup2 error");
		exit(1);
	      }
	    } else {
	      //if there was no filepath then print error message and send exit status of 1
	      perror("No filepath provided for input redirect");
	      exit(1);
	    }
	    //At this point I belive we have succesfully redirected stdin to be from the file
	  }

	  //NOTE THIS: to disable redirects have added an impossible condition here
	  if (curCmd->outRedirect == 1 && 1 == 2){
	    //we have a command that is attempting to redirect output to file

	    //process mirrors above process, start with checking file path exists
	    if (curCmd->outFilePath) {
	      
	      //Now attempt to open file
	      targetFD = open(curCmd->outFilePath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	      if (targetFD == -1) {
		perror("cant write to file");
		exit(1);
	      }
	      dupOutRes = dup2(targetFD, 1);
	      if (dupOutRes == -1){
		//error with dup2 on write
		perror("dup2 error output redirect");
		exit(1);
	      }
	    } else {
	      //no filepath provided so error
	      perror("No filepath provided for output redirect");
	      exit(1);
	    }
	    //At this point we should have successfully redirected standard out
	  }
	  //NOTE THIS: disabled with impossible condition
	  if (curCmd->inRedirect == 1 && 1 == 2) {
	    //if we have input redirect we dont want to use options to determine inputs
	    execlp(curCmd->name, curCmd->name, NULL);//this should use our stdin stdout as set
	  }
	  else {
	    //if we didnt have input redirect, options come from command struct
		sourceFD = open("/dev/null", O_RDONLY);
		targetFD = open("dev/null", O_WRONLY, 0644);
		if (sourceFD == -1 || targetFD == -1){
			perror("couldnt set bg in/out to /dev/null");
			exit(1);
        }
		dupResult = dup2(sourceFD, 0);
		dupOutRes = dup2(targetFD, 0);
		if (dupResult == -1 || dupOutRes == -1){
			perror("issue with dup2 on bg process");

        }
	    execvp(curCmd->options[0], curCmd->options);
	  }
	  perror("exec func failed");//writing error message
	  exit(1);
	  break;
	  
	} else {
	  //we are in the parent process
	//We want to first add a node to our linked list of bg processes with this process's ID
	  curNode = headNode;
	  while (curNode->nextNode){
		curNode = curNode->nextNode;
      }
	  struct bgProcessNode *newNode = malloc(sizeof(struct bgProcessNode));
	  newNode->processId = spawnPid;
	  newNode->nextNode = NULL;
	  spawnPid = waitpid(spawnPid, &childStatus, WNOHANG);
	  char *tempStringedPid = intToStr(spawnPid);
	  write(STDOUT_FILENO, "background pid is ", 19);
	  write(STDOUT_FILENO, tempStringedPid, myStrLen(tempStringedPid) + 1);
	  fflush(stdout);
	  free(tempStringedPid);
	  /* We will deal with status results differently for WNOHANG
	  if (childStatus != 0){
	    curStatus = 1;
	  }
	  else {
	    curStatus = 0;
	  }*/
	  

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
