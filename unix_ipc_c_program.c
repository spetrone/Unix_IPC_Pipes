/*
 * --> The program creates two childprocesses. 
 * -->The first child process checks if file exists in filesystem and communicates the outcome
 * to the second child process via a pipe and exits.
 * --> The second child process will then either create the folder if it does not exist or
 * perform no action if the folder already exists.
 * --> second child process sends a confirmation message to the parent
 *  process via another pipe and then exits. 
 * -->The parent process then checks for contents in the folder. If there is content in this folder, the parent process
 * deletes all content and prints a message for the user upon successful completion
 * and terminates. 
*-->All printing, input, and reading (from filesystem or the console)
 * should be done by system calls. */

#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <dirent.h>

#define MSGSIZE 32

int main (int argsc, char* argv[]) {

	pid_t child1;
	pid_t child2;
	char msg[128];
	int pcPipe[2]; //file descriptors for pipe between parent and child2
	int childPipe[2]; //file descriptors for pipe between child processes
	char inBuf[MSGSIZE]; //buffer for pipe
	char inBuf2[MSGSIZE]; //buffer for pipe between parent and child
	int nbytes; //will hold number of bytes from read operations with pipe
	char folderName[128]; //holds foldername up to 128 chars
	struct stat statBuf;		     

	int dirCount = 0; //used in parent for counting content in directory
	DIR *dir; //for using readdir in parent
	struct dirent* next; //for iterating through directory contents
	char command[128]; //for building a command to delete directory contents

	//ensure correct number of commands given
	if (argsc != 2) {
		strcpy(msg, "requires usage: ./emptyFolders foldername\n"); 
		write(2, msg, strlen(msg)+1);
		exit(1);
	}
	
	//read foldername from command line
	strcpy(folderName, argv[1]);
	


	//create pipe for child process 
	if (pipe(childPipe) < 0) {
		strcpy(msg, "failed to create pipe for child 1 and child 2\n");
		write(2, msg, strlen(msg)+1);
		exit(1); 
	}


	//create first child process
	if ((child1 = fork()) == -1) {
		strcpy(msg, "failed to create first child process with fork()\n");
		write(2, msg, strlen(msg)+1);
		exit(1);
	}

	
	//do operations in first child process
	if (child1 == 0){ //(in child1 process)
		
		close(pcPipe[1]); //close write end of the pipe to parent
	
		//test if arg is a directory and exists
		if (stat(folderName, &statBuf) == 0 && S_ISDIR(statBuf.st_mode)) {
			//it is a directory, communicate with other child process
			//passes "t" as message to other child saying that the directory exists
			if( (write(childPipe[1], "t", 2)) < 0) {
				strcpy(msg, "error writing true from child1 to child 2; from errno: ");
				strcat(msg, strerror(errno));
				write(2, msg, strlen(msg) +1);
				exit(1);
			}
		}		
		else { //not a directory, must create it
		       //passes "f" to other child to say that the directory does not exist
			if( (write(childPipe[1], "f", 2)) < 0) {
				strcpy(msg, "error writing false from child1 to child 2; from errno: ");
				strcat(msg, strerror(errno));
				write(2, msg, strlen(msg)+1);
				exit(1);
			}
		}

		close(childPipe[1]); //close pipe to prevent read from blocking

	}//end if child1
	else {
		close(childPipe[1]); //close write end of pipe with child, prevents blocking

		//create pipe for parent and child 2
		if (pipe(pcPipe) < 0) {
			strcpy(msg, "failed to create pipe for parent and child 2\n");
			write(2, msg, strlen(msg)+1);
			exit(1); 
		} 

		//create another child processs	
		if ((child2 = fork()) == -1) {
			strcpy(msg, "failed to create second child process with fork()\n");
			write(2, msg, strlen(msg)+1);
			exit(1);
		}
	  
		//in the second child, the  process will read from pipe with child1 and create folder if necessary
		  if (child2 == 0) {

			  close(childPipe[1]); //close to prevent read from blocking

			  //get info from child1 process
			  while ((nbytes = read(childPipe[0], inBuf, MSGSIZE)) > 0) {

				  //if value is "t", then do nothing, otherwise, if "f" make directory; otherwie error
				  if(strcmp(inBuf, "t") == 0) {
					strcpy(msg, "folder exists\n");
					write(1, msg, strlen(msg)+1);

					//communicate with pipe to parent that its operations are complete
					//with "done" message
					if( (write(pcPipe[1], "done", 5)) < 0) {
						strcpy(msg, "error writing done from child 2 to parent; from errno: ");
						strcat(msg, strerror(errno));
						write(2, msg, strlen(msg)+1);
						exit(1);
					}

				  }
				  else if(strcmp(inBuf, "f") == 0 ) {
					  //create folder
					  strcpy(msg, "creating new folder...\n");
					  write(1, msg, strlen(msg)+1);

					  if (mkdir(folderName, 0755) < 0) {
						strcpy(msg, "error creating directory with mkdir in child2\n");
						write(2, msg, strlen(msg)+1);
						exit(1);
					  }

					//communicate with pipe to parents that its operations are complete
					//with "done" message  
					if( (write(pcPipe[1], "done", 5)) < 0) {
						strcpy(msg, "error writing done from child 2 to parent; from errno: ");
						strcat(msg, strerror(errno));
						write(2, msg, strlen(msg)+1);
						exit(1);
					}
				  }
				  else { //error
					strcpy(msg, "Error with child2, not true or false from child1\n");
					write(2, msg, strlen(msg)+1);
					exit(1);
				  }
			  }
			  
			  close(pcPipe[1]); //close write end of pipe to prevent blocking in parent

		  } //end if child2 process
		  else { 
			  //in PARENT process, read from pipe to child2 for "done" message 
			  while((nbytes = read(pcPipe[0], inBuf2, MSGSIZE)) > 0) {
				  
				  close(pcPipe[1]);
				  
				  while(strcmp(inBuf2, "done") != 0) {
					  //...wait for done message in pipe from child
				  }

				  
				  if((dir = opendir(folderName)) == NULL ) {
				  	strcpy(msg, "Error opening directory in parent; err:  \n");
					strcat(msg, strerror(errno));
					write(2, msg, strlen(msg)+1);
					exit(1);
				  } 
				  else {
					//count files in directory onnly up to three (should only be . and .. in directory)
				  	while ((next = readdir(dir)) != NULL && dirCount < 4) {
						dirCount++;
					}
					//if more than two (. and ..), has content; delete contents
					if(dirCount > 2) {
						strcat(command, "exec rm -r ");
						strcat(command, folderName);
						strcat(command, "/*");
						system(command);

						strcpy(msg, "Folder not empty, removed all content...\n");
						write(1, msg, strlen(msg)+1);
					}
					else {
						strcpy(msg, "Folder is already empty...done\n");
						write(1, msg, strlen(msg)+1);
					}
			          }
		          }//end while (read operation from child)

		  } //end else, in parent process  

	} //end else, not child1 process
	       

	return 0;

}
