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



//------------Program Description--------------
//This program is a homemade shell.  It has basic functionality, with handwritten status, exit and cd functions
//The ability to call default shell function through exec calls
//And some simple signal interupt functionality for ctrl-c on foreground processes terminating them
// and ctrl-z or any SIGTSTP signal which will cause the terminal to enter or leave foreground only mode.
// Also has functionality for input redirect with <, output redirect with >, and background command running
// which happens by giving & as the final input to a command (but will not work if in fg only mode).

//Global variables
//Cur status will always be an int that is the status of the most recent exit code of any process
int curStatus = 0;
int fgOnlyMode = 0;//This becomes 1 if we are in fg only mode, is 0 if not.
pid_t fgProcessId = 0;//Should be 0 unless we are currently running a process in the foreground
//fgProcessId is how our SIGTSTP handler knows if it needs to wait for a process to finish.

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
    struct bgProcessNode *nextNode;
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

  //Iterate through the number digit by digit copying the current last digit into the string
  //and then removing the last digit by subtracting it and dividing by 10
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

  //Set tempChar to store the character that gets overwritten first as we reverse the string
  char tempChar;
  while (count > reverseCount){//We only need to iterate through half of the string since we are switching front and back correspondants
    tempChar = pidStr[count];
    pidStr[count] = pidStr[reverseCount];
    pidStr[reverseCount] = tempChar;
    reverseCount++;
    count = count -1;
  }

  //Finally we copy the string into heap memory to be returned
  char *result = calloc(myStrLen(pidStr) + 1, sizeof(char));
  strcpy(result, pidStr);
  return result;
}


//--------sigintHandler function-------
//This function is used by a child process killed by a SIGINT signal

void sigintHandler(int signum){
  //when this function is called we want to:
  //terminate the child process that is calling it
  //pass the number of the signal that killed this process back to the parent process
  exit(signum);//Exit both terminates the current process and returns the signum for us
}


//-------sigtstpHandler function-------
//This function is called by shell when entering or leaving fg only mode with a SIGTSTP

void sigtstpHandler(int signum){
  //When this function gets called we know we are in the parent process that is running the shell
  //We want to immediately display a message (or display it after the child fg process, which shoudl happend naturally)
  //Then switch the current fg-only mode (so if on turn off, if off turn on)
  if (fgOnlyMode == 0){
    //we are currently not in fg only mode, so need to display message that we are going into it and go into it
    //Before displaying the message though we want to wait for any fg process to finish
    if (fgProcessId > 0){
      int childStatus;
      waitpid(fgProcessId, &childStatus, 0);
    }
    write(STDOUT_FILENO, "\nEntering foreground-only mode (& is now ignored)\n", 51);
    fflush(stdout);
    fgOnlyMode = 1;
  } else if (fgOnlyMode == 1) {
    //We are in fg only mode, so want to display message and get out of it
    //Before displaying the message though we want to wait for any fg process to finish
    if (fgProcessId > 0){
      int childStatus;
      waitpid(fgProcessId, &childStatus, 0);
    }
    write(STDOUT_FILENO, "\nExiting foreground-only mode\n", 31);
    fflush(stdout);
    fgOnlyMode = 0;
  }

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
  //Also, I don't need to worry about the potential of converting negative numbers since Pid will always be positive
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
  //At this point pidStr is a string that is the pid, so we can use it to replace double dollar signs

  //Here we loop while there are "$$" in the source string and replace them with the pid string
  int lenPidStr = myStrLen(pidStr);
  char *varIndex = strstr(resultStr, "$$");
  while (varIndex){//If varIndex is not null the strstr found a "$$"
    //Start by designating an area on the heap big enough to hold the result string with additional characters of pidStr
    tempStr = calloc(myStrLen(resultStr) + lenPidStr + 1, sizeof(char));//This is slightly larger than it needs to be, but better safe than sorry

    for (int i = 0; i < (varIndex - resultStr); i++){
      tempStr[i] = resultStr[i];
    }//This for copies over the string up to the $$
    strcat(tempStr, pidStr);//This appends the pid
    strcat(tempStr, (varIndex + (2 * sizeof(char))));//this copies the rest of string after $$
    free(resultStr);//Release the memory of resultStr, so we can then copy the tempStr in
    resultStr = calloc(myStrLen(tempStr) + 1, sizeof(char));
    strcpy(resultStr, tempStr);
    free(tempStr);//We have copied into resultStr so we can release tempStr
    varIndex = strstr(resultStr, "$$");
  }//loop will repeat if there are any more "$$" in resultStr


  return resultStr;
}



//------readCommand function------
//This function is the top level function that gets called when we recieved a command
//Its purpose is to create a cmdStruct that has all the info we need for determining how to handle the command

struct cmdStruct *readCommand(char *inStr){
  //Initialize and designate heap space for the cmdStruct, as well as declare our tokenizing variables
  struct cmdStruct *parsedCmd = malloc(sizeof(struct cmdStruct));
  char *savePtr;
  char *curTok = strtok_r(inStr, " \n", &savePtr);
  int typeNextTok = 1;
  /* typeNextTok is how I tell the loop what it expects to see in the next token
     1 means the next token will be the command
     2 means we are expecting options
     3 means we encountered "<"
     4 means we encountered ">"
     5 means we encountered "&" (5 as an option is not used in current version, but leaving this note if we reintroduce it later)
     6 means we have taken an input redirect and will only accept an output redirect or "&"
     7 means we have taken an output redirect and will only accept input redirection or "&"
  */
  //SPECIAL NOTE: Our program will only allow one input redirect and one output redirect, but will not create an error
  //if multiple are requested.  It will simply use the last instance of each.

  //Now we set default values of certain members of cmdStruct
  parsedCmd->fgOrBg = 1; //default to running in fg
  parsedCmd->numOptions = 0; //assumes no options to start with
  parsedCmd->inRedirect = 0; //assumed no input redirect until it encounters <
  parsedCmd->outRedirect = 0; //assumes no output redirect until it encounter >
  parsedCmd->validInput = 1;//Assumes the input will be valid, this is set to 0 if we parsed the input but find it invalid


  //Loop creating tokens for each space separated element of the input string
  while(curTok != NULL) {
    
    if (typeNextTok == 1) {
      //We are reading in the primary command.
      if (curTok[0] == '#'){
	break;//We just break out of the loop if comment and let comment catcher after loop deal with parsing
      } else {
	//Need to replace any double $ with pid
	curTok = varExpand(curTok);

	//Store the name of the command we are running, and set typeNextTok to 2 so we read next vals as options
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
      //Start by checking if our current option is one of the special symbols for redirection of bg process
      if (strcmp(curTok, "<") == 0 && (strcmp(parsedCmd->name, "exit") != 0 && strcmp(parsedCmd->name, "cd") != 0 
				       && strcmp(parsedCmd->name, "status") != 0)) {//dont allow redirect for builtins
	parsedCmd->inRedirect = 1;
	typeNextTok = 3;
      } else if (strcmp(curTok, ">") == 0 && (strcmp(parsedCmd->name, "exit") != 0 && strcmp(parsedCmd->name, "cd") != 0 
					      && strcmp(parsedCmd->name, "status") != 0)){//dont allow redirect for builtins
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
	  //Note: token has already moved on so we add a "&" string literal, and continue so we dont increment
	  //the token again, since we already incremented it to check if we are at the end.
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
      //now would be a "&" command to indicate run in bg or an input redirect
      if(strcmp(curTok, "&") == 0) {
	
	//check that this is the final character by getting next token
	curTok = strtok_r(NULL, " \n", &savePtr);
	if(curTok == NULL) {
	  parsedCmd-> fgOrBg = 0;
	  break;
	}
      } else if (strcmp(curTok, "<") == 0){
	//Now we know our next input will be an input redirect
	parsedCmd->inRedirect = 1;
	typeNextTok = 3;
      } else {
      
      //if we get here then there are invalid input fields at the end of our input string
      //So I indicate this with the validInput boolean in the parseCmd
      parsedCmd-> validInput = 0;
      break;
      }
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

  //Finally, we have a strange error where options that were part of previous commands
  //Seem to linger into to new commands somehow.  So we set the option at index
  //numOptions to NULL so that the exec functions will know not to read any ghost options
  parsedCmd->options[parsedCmd->numOptions] = NULL;

  return parsedCmd;
}





//------main function------
//Main holds the shell loop, signal handling, declaration of bg process linked list
//and the processing of commands.  It is a forking parent process that spawns child processes
//when the commands given are not commands that we deal with in our custom functionality
// by calling exec functions after forking.

int main(int argc, char *argv[]){
  
  //Define variable that will be used throughout to determine if we should keep the shell running
  int keepRunning = 1;
  //Define the variables that all of main needs access to
  char *inputString;//will store user inputs
  int forkedId;
  struct cmdStruct *curCmd;//will store the parsed command structure based on user inputs
  sigset_t signalSet;//Defining signal set to just include the signals that I accept
  sigemptyset(&signalSet);
  if (sigaddset(&signalSet, SIGINT) == -1){
    perror("failed to add SIGINT");
  }
  if (sigaddset(&signalSet, SIGTSTP) == -1){
    perror("failed to add SIGTSTP");
  }
  
  //The following code is based on the exploration for Signal Handling in Module 5
  //Website: https://canvas.oregonstate.edu/courses/1830250/pages/exploration-signal-handling-api?module_item_id=21468881
  struct sigaction SIGINT_action = {0};
  //filling in structure
  //Want to register ignore as the default action, and then register our special handler
  //if we are in a child running in the fg
  SIGINT_action.sa_handler = SIG_IGN;
  //block all catchable signals while handling a SIGINT
  sigfillset(&SIGINT_action.sa_mask);
  //no flags set
  SIGINT_action.sa_flags = 0;
  //install the signal handler
  sigaction(SIGINT, &SIGINT_action, NULL);
  
  //Now we set the handler to sigtstpHandler for SIGTSTP since this is parent process
  //We will later set it to ignor anytime we are in a child process
  struct sigaction SIGTSTP_action = {0};
  SIGTSTP_action.sa_handler = sigtstpHandler;
  sigfillset(&SIGTSTP_action.sa_mask);
  SIGTSTP_action.sa_flags = 0;
  sigaction(SIGTSTP, &SIGTSTP_action, NULL);


  //Initialize variables for tracking bg processes
  struct bgProcessNode *headNode = malloc(sizeof(struct bgProcessNode));
  //headNode will always point to our head sentinel so we can be sure to iterate through the whole list when we need
  headNode->processId = -1;
  headNode->nextNode = NULL;
  struct bgProcessNode *curNode = malloc(sizeof(struct bgProcessNode));//used to store current node when iterating
  struct bgProcessNode *prevNode = malloc(sizeof(struct bgProcessNode));//used to store previous node when iterating
  
  //Also declare variable for monitoring child forking bg processes
  int bgChildStatus;
  pid_t bgChildPid;


  //Main loop, will ask for a prompt by writing : and waiting for input
  while (keepRunning == 1)
    {
    //allocate memory for the input
    inputString = calloc(2049, sizeof(char));//2049 to allow input stirngs of up to 2048 chars

    //Reset curNode and iterate through nodes checking if bgProcess completed
    curNode = headNode;
    //bgChildStatus = 0;
    while(curNode->nextNode){
    //If there is a next node then check if that process is complete
     
      prevNode = curNode;//store curNode before going to next so we can set next of previous to currents next when deleting
      curNode = curNode->nextNode;
      bgChildPid = waitpid(curNode->processId, &bgChildStatus, WNOHANG);
      if(WIFEXITED(bgChildStatus) && bgChildPid > 0){
        //Here we have exitted normally from a bg process
	//want to display the info and the exit value
	write(STDOUT_FILENO, "background pid ", 16);
        char *pidStrTemp = intToStr(bgChildPid);
        write(STDOUT_FILENO, pidStrTemp, myStrLen(pidStrTemp) + 1);
        write(STDOUT_FILENO, " is done: exit value ", 22);
	free(pidStrTemp);//need to make sure to free our strings so we dont have memory leaks
	pidStrTemp = intToStr(bgChildStatus);
	write(STDOUT_FILENO, pidStrTemp, myStrLen(pidStrTemp) + 1);
	write(STDOUT_FILENO, "\n", 2);
        fflush(stdout);
	free(pidStrTemp);
	prevNode->nextNode = curNode->nextNode;
	free(curNode);
       
      } else if (WIFSIGNALED(bgChildStatus) && bgChildPid > 0){
	//Here we have exitted abnormally (probably by process being terminated)
	//want to display a special message with pid and termination status
	write(STDOUT_FILENO, "background pid ", 16);
        char *pidStrTemp = intToStr(bgChildPid);
        write(STDOUT_FILENO, pidStrTemp, myStrLen(pidStrTemp) + 1);
        write(STDOUT_FILENO, " is done: terminated by signal ", 32);
	free(pidStrTemp);
	pidStrTemp = intToStr(bgChildStatus);
	write(STDOUT_FILENO, pidStrTemp, myStrLen(pidStrTemp) + 1);
	write(STDOUT_FILENO, "\n", 2);
        fflush(stdout);
	free(pidStrTemp);
	prevNode->nextNode = curNode->nextNode;
	free(curNode);
      }
    }
    
    
    //Display the prompt colon.
    write(STDOUT_FILENO, ":", 2);
    fflush(stdout);
    
    //Get user input and process it into a command structure.
    fgets(inputString, 2049, stdin); 
    curCmd = readCommand(inputString);
    

    //Handling built-in commands
    if (strcmp(curCmd->name, "exit")==0){
      keepRunning = 0;
      //kill all child processes
      curNode = headNode;
      while(curNode->nextNode){
	//loop while there are on completed child processes
	prevNode = curNode;
	curNode = curNode->nextNode;
	//kill each child process with the kill command
	kill(curNode->processId, SIGKILL);
	free(prevNode);//free up the memory for each node, this is slightly unnecessary since the program will
	               // free up all memory when it exits, but doing this just to be thorough
	               // and to make the functionality more complete in case I want to use code later and change things
      }
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
	  //path is relative, so we designate a heap pointer with enough space for the cwd and the relative path
	  char *tempPath = calloc(myStrLen(curCwd) + myStrLen(curCmd->options[1]) + 2, sizeof(char));//+2 for extra "/" and null terminator
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
      if (curStatus < 2){
	//if our status is not 2 or greater (which means a signal interrupt happened)
	// then we want to just display a regular status message starting with exit value
	write(STDOUT_FILENO, "exit value ", 12);
	char* strStat = intToStr(curStatus);
	write(STDOUT_FILENO, strStat, myStrLen(strStat) + 1);
	write(STDOUT_FILENO, "\n", 2);
	fflush(stdout);
	free(strStat);
      } else {//This means we had a signal interrupt
	//We want to display the special message format of terminate by signal
	write(STDOUT_FILENO, "terminated by signal ", 22);
	char* strStat = intToStr(curStatus);
	write(STDOUT_FILENO, strStat, myStrLen(strStat) + 1);
	write(STDOUT_FILENO, "\n", 2);
	fflush(stdout);
	free(strStat);
      }

    } else {//Split off for non-built-ins
      
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
	  
	  //First thing we do is change the handler for SIGINT to our sigintHandler function
	  //So that a fg process can be killed with ctrl-c
	  SIGINT_action.sa_handler = sigintHandler;
	  //Re-installing to make sure it is updated
	  sigaction(SIGINT, &SIGINT_action, NULL);
	  
	  //Now we need to set the handler for SIGTSTP to SIG_IGN
	  SIGTSTP_action.sa_handler = SIG_IGN;
	  sigaction(SIGTSTP, &SIGTSTP_action, NULL);

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

	  //Now we want to actually run the command using exec, passing our option array and the command
	  execvp(curCmd->options[0], curCmd->options);
	  //We will only get past the above call if there is an error
	  perror("exec func failed");//writing error message
	  exit(1);
	  break;
	  
	} else {
	  //we are in the parent process
	  //First we want to store the pid of the child to the fgProcessId variable
	  //so that SIGTSTP will function correctly
	  //Then we will wait on the child process completing
	  fgProcessId = spawnPid;
	  spawnPid = waitpid(spawnPid, &childStatus, 0);

	  //Now we want to set fgProcessId back to 0 because child process is done
	  fgProcessId = 0;
	  if (childStatus == 0){//setting status to 0 if no error
	    curStatus = 0;
	  }
	  else if (childStatus ==2 || childStatus == 3 || childStatus == 9 
		   || childStatus == 15 || childStatus == 24
		   || childStatus == 25) {//checking if it is any valid interupt signal that would terminate
	    //If we got a valid interupt we want to display a termination message
	    write(STDOUT_FILENO, "terminated by signal ", 22);
	    char * tempNumStr = intToStr(childStatus);
	    write(STDOUT_FILENO, tempNumStr, myStrLen(tempNumStr) + 1);
	    write(STDOUT_FILENO, "\n", 2);
	    fflush(stdout);
	    free(tempNumStr);
	    curStatus = childStatus;//Here we set the current status, which will be greater than 1
	    //So if we run our custom status command after this it will print the special termination message
	  }
	  else {
	    curStatus = 1;
	  }
	}
	
      } else {//this is the else that means we need to run as bg process
	
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
	
	  //We need to update any signal handling, in this case just SIGTSTP
	  //we set the handler for SIGTSTP to SIG_IGN
	  SIGTSTP_action.sa_handler = SIG_IGN;
	  sigaction(SIGTSTP, &SIGTSTP_action, NULL);

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

	  //I used to have different exec call for redirect or non-redirect, which would happen here
	  //But believe that is unneccessary.  I leave this note about it though in case
	  //That is something I need to add back in later.

	  //if we didnt have input redirect, we want the input to source from dev/null
	  //so we aren't reading anything from the fg processes
	  if (curCmd->inRedirect == 0){
	    sourceFD = open("/dev/null", O_RDONLY);
	    if (sourceFD == -1){
	      perror("couldnt set bg input redirect to /dev/null");
	      exit(1);
	    }
	    dupResult = dup2(sourceFD, 0);
	    if (dupResult == -1){
	      perror("dup2 issue bg /dev/null input redirect");
	      exit(1);
	    }
	  }

	  //if we didnt have output redirect, then we want to redirect out to dev/null
	  //So that we dont have output from bg process in terminal
	  if (curCmd->outRedirect == 0){
	    targetFD = open("/dev/null", O_WRONLY, 0644);
	    if (targetFD == -1){
	      perror("couldnt set bg out to /dev/null");
	      exit(1);
	    }
		
	    dupOutRes = dup2(targetFD, 1);
	    if (dupOutRes == -1){
	      perror("issue with dup2 on bg process");
	      exit(1);
	    }
	  }

	  //This is where we actually call the command
	  execvp(curCmd->options[0], curCmd->options);
	  //we won't get here unless there is an error
	  perror("exec func failed");//writing error message
	  exit(1);
	  break;
	  
	} else {//parent else
	  //we are in the parent process
	//We want to first add a node to our linked list of bg processes with this process's ID
	  curNode = headNode;
	  while (curNode->nextNode){
		curNode = curNode->nextNode;
	  }
	  struct bgProcessNode *newNode = malloc(sizeof(struct bgProcessNode));
	  newNode->processId = spawnPid;
	  newNode->nextNode = NULL;
	  curNode->nextNode = newNode;//Add the new node to our linked list of bg processes

	  //Now we create our background process message using the spawnPid to know the child's pid
	  char *tempStringedPid = intToStr(spawnPid);
	  write(STDOUT_FILENO, "background pid is ", 19);
	  write(STDOUT_FILENO, tempStringedPid, myStrLen(tempStringedPid) + 1);
	  write(STDOUT_FILENO, "\n", 2);
	  fflush(stdout);
	  free(tempStringedPid);
	  //We dont use any sort of wait command because our shell checks for process termination
	  //Before taking inputs and we are running the child in the bg so don't need to track its
	  //results here.
	}
	
      }
    }

    //de-allocate memory for the input
    free(inputString);
    //de-allocate memory for the command
    free(curCmd->name);
    int clearCounter = 0;
    while (clearCounter < curCmd->numOptions){
      //Iterating through the options attribute to clear out any input options
      free(curCmd->options[clearCounter]);
      clearCounter++;
    }

    free(curCmd);
    }

    return EXIT_SUCCESS;
    
}
