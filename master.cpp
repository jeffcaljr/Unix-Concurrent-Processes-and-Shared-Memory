/**
Author: Jeffery Calhoun
Course: CS 4760 Operating Systems Section 2 (TTh 9:30-10:45am)
Date: 10/05/2016
*/

#include <unistd.h>
#include <fstream> 
#include <iostream>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <cstdlib>
#include <cstring>

using namespace std;

void spawnChild(int count); //spawns child if less than 20 processes are in the system
void spawn(int count); //helper function of spawnChild for code simplicity
void sigHandler(int SIG_CODE); //Signal handler for master to handle Ctrl+C interrupt
void timerSignalHandler(int); //Signal handler for master to handle time out
void releaseMemory(); //Releases all shared memory allocated

const int MAX_NUM_OF_PROCESSES_IN_SYSTEM = 20;
int currentNumOfProcessesInSystem = 0; //FIXME: Does this count include the parent 

//Shared memory key, id, and data for sharedNum to be incrememnted by children
int sharedIntKey = ftok("Makefile", 1);
int sharedIntSegmentID;
int *sharedInt;

//Shared memory keys, ids, and data stores used for Peterson's algorithm flags array
int flagsKey = ftok("Makefile", 2); //
int flagsSegmentID;
int *flags;

//Shared memory keys, ids, and data stores used for Peterson's algorithm turn variable
int turnKey = ftok("Makefile", 3);
int turnSegmentID;
int *turn;

//Shared memory key, id, and data store used to share slave process group id
//this group id is retrieved from the first spawned slave and set to subsequent spawns
//to allow for sending kill signal to all slaves
int slaveProcessGroupKey = ftok("Makefile", 4);
int slaveProcessGroupSegmentID;
pid_t * slaveProcessGroup;


//Shared memory key, id, and data store used to share number of slaves to spawn
int slaveKey = ftok("Makefile", 5);
int slaveSegmentID;
int *slaveNum;

//Shared memory key, id, and data store used to share output file name
int fileNameKey = ftok("Makefile", 6);
int fileNameSegmentID;
char *fileName;


//Shared memory key, id, and data store used to share number of times slave should enter crit. sec.
int maxWritesKey = ftok("Makefile", 7);
int maxWritesSegmentID;
int *maxWrites;

int status = 0; //used for wait status in spawnChild function

int startTime; //will hold time right before forking starts, used in main and timer signal handler
int durationBeforeTermination; //how long should master run? set in main, used in timer signal handler


int main(int argc, char** argv){

	//Register signal handlers
	signal(SIGINT, sigHandler);
	signal(SIGUSR2, timerSignalHandler);

	int numOfSlaves = 5; //number of slaves user wants to spawn
	int numOfSlaveExecutions = 3; //Number of times slaves will execute their critical sections
	durationBeforeTermination = 20; //Number of seconds before master terminates itself
	const char *fName = "test.out"; //Output file name slave processes will write to

	extern char *optarg; //used for getopt
	int c; //used for getopt

	int x, y, z; //temporary variables for command line arguments

	while((c = getopt(argc, argv, "hs:l:i:t:")) != -1){
		switch(c){
			case 'h': //help option
				cout << "This program accepts the following command-line arguments:" << endl;
				cout << "\t-h: Get detailed information about command-line argument options." << endl;
				cout << "\t-s x: Specify maximum number of slave processes to spawn (default 5)." << endl;
				cout << "\t-l filename: Specify the output file for the log (default 'test.out')." << endl;
				cout << "\t-i y: Specify number of times each slave should execute critical section (default 3)." << endl;
				cout << "\t-t z: Specify time (seconds) at which master will terminate itself (default 20)." << endl;

				exit(0);
			break;

			case 's': //# of slaves option
				x = atoi(optarg);
				if(x < 0){
					cerr << "Cannot spawn a negative number of slaves." << endl;
					exit(1);
				}
				else{
					numOfSlaves = x;
				}
			break;

			case 'l': //filename option
				fName = optarg;
			break;

			case 'i': //Number of critical section executions by slaves
				y = atoi(optarg);
				if(y < 0){
					cerr << "Negative arguments are not valid" << endl;
					exit(1);
				}
				else{
					numOfSlaveExecutions = y;
				}
			break;

			case 't': //time at which master will terminate
				z = atoi(optarg);

				if(z < 0){
					cerr << "Master cannot have a run duration of negative time." << endl;
					exit(1);
				}
				else{
					durationBeforeTermination = z;
				}
			break;

			default:
				cerr << "Default getopt statement" << endl; //FIXME: Use better message.
				exit(1);
			break;
		}

	}

	//Configure shared memory for sharedNum
	if((sharedIntSegmentID = shmget(sharedIntKey, sizeof(int), IPC_CREAT|S_IRUSR | S_IWUSR)) < 0){
	 	perror("shmget: Failed to allocate shared memory for shared int");
	 	exit(1);
	 }
	 else{
	 	sharedInt = (int *) shmat(sharedIntSegmentID, NULL, 0);
	 	(*sharedInt) = 0;
	 }

	 //configure shared memory for Peterson's algorithm arrays
	if((flagsSegmentID = shmget(flagsKey, numOfSlaves * sizeof(int), IPC_CREAT|S_IRUSR | S_IWUSR)) < 0){
	 	perror("shmget: Failed to allocate shared memory for flags array");
	 	exit(1);
	 }
	 else{
	 	flags = (int *)shmat(flagsSegmentID, NULL, 0); //Should this be cast to something else
	 }

	if((turnSegmentID = shmget(turnKey, sizeof(int), IPC_CREAT|S_IRUSR | S_IWUSR)) < 0){
	 	perror("shmget: Failed to allocate shared memory for turn array");
	 	exit(1);
	 }
	 else{
	 	turn = (int *)shmat(sharedIntSegmentID, NULL, 0);
	 }

	//Congifure shared memory to hold PID of first slave. This will be used as a groupPID for all slaves
	
	if((slaveProcessGroupSegmentID = shmget(slaveProcessGroupKey, sizeof(pid_t), IPC_CREAT|S_IRUSR | S_IWUSR)) < 0){
	 	perror("shmget: Failed to allocate shared memory for group PID");
	 	exit(1);
	 }
	 else{
	 	slaveProcessGroup = (pid_t *) shmat(slaveProcessGroupSegmentID, NULL, 0);
	 }


	//Congifure shared memory to hold num of slaves to spawn
	if((slaveSegmentID = shmget(slaveKey, sizeof(int), IPC_CREAT|S_IRUSR | S_IWUSR)) < 0){
	 	perror("shmget: Failed to allocate shared memory for slaveNum");
	 	exit(1);
	 }
	 else{
	 	slaveNum = (int *)shmat(slaveSegmentID, NULL, 0);
	 	*slaveNum = numOfSlaves;
	 }

	 
	 //Congifure shared memory to hold fileName
	 //Assumed to be less than 25 characters
	 
	if((fileNameSegmentID = shmget(fileNameKey, sizeof(char) * 26, IPC_CREAT|S_IRUSR | S_IWUSR)) < 0){
	 	perror("shmget: Failed to allocate shared memory for filename");
	 	exit(1);
	 }
	 else{
	 	fileName = (char *)shmat(fileNameSegmentID, NULL, 0);
	 	
	 	strcpy(fileName, fName);
	 }

	 //Congifure shared memory to hold num of times slaves should execute critical section
	if((maxWritesSegmentID = shmget(maxWritesKey, sizeof(int), IPC_CREAT|S_IRUSR | S_IWUSR)) < 0){
	 	perror("shmget: Failed to allocate shared memory for maxWrites");
	 	exit(1);
	 }
	 else{
	 	maxWrites = (int *)shmat(maxWritesSegmentID, NULL, 0);
	 	*maxWrites = numOfSlaveExecutions;
	 }

	 //start timer, then spawn slaves
	 startTime = time(0);

	 int count = 1;

	 while(count <= numOfSlaves){
	 	spawnChild(count);
	 	++count;
	 }

	 //wait for all child processes to finish or time to run out, then free up memory and close

	while((time(0) - startTime < durationBeforeTermination) && currentNumOfProcessesInSystem > 0){
	 	wait(NULL);
	 	--currentNumOfProcessesInSystem;
	 	cout << currentNumOfProcessesInSystem << " processes in system.\n";
	 }

	releaseMemory();

	return 0;
}

void spawnChild(int count){
	if(currentNumOfProcessesInSystem < MAX_NUM_OF_PROCESSES_IN_SYSTEM){
		spawn(count);
	}
	else{
		waitpid(-(*slaveProcessGroup), &status, 0);
		--currentNumOfProcessesInSystem;
		cout << currentNumOfProcessesInSystem << " processes in system.\n";
		spawn(count);
	}
}

void spawn(int count){
	++currentNumOfProcessesInSystem;
	if(fork() == 0){
		cout << currentNumOfProcessesInSystem << " processes in system.\n";
	 	if(count == 1){ //only set slaveProcessGroup for first process //
	 		(*slaveProcessGroup) = getpid();
	 	}
	 	setpgid(0, (*slaveProcessGroup));
	 	execl("./slave", "slave", to_string(count).c_str(), (char *)NULL); //FIXME: needs to pass in numOfExecutions as parameter instead of 4
	 	exit(0);
	 }
}

void sigHandler(int signal){
	killpg((*slaveProcessGroup), SIGTERM);
	//CHECK TO SEE IF PROCESSES HAVE EXITED with waitpid and sigkill
	// sleep(3);
	for(int i = 0; i < currentNumOfProcessesInSystem; i++){
	 	wait(NULL);
	 }
	releaseMemory();
	// file->close();
	cout << "Exiting master process" << endl;
	exit(0);
}

void timerSignalHandler(int signal){
	if(time(0) - startTime >= durationBeforeTermination){
	 	cout << "Master: Time's up!\n";
	 	killpg((*slaveProcessGroup), SIGUSR1);
		//CHECK TO SEE IF PROCESSES HAVE EXITED with waitpid and sigkill
		// sleep(3);
		for(int i = 0; i < currentNumOfProcessesInSystem; i++){
		 	wait(NULL);
		 }
		releaseMemory();
		cout << "Exiting master process" << endl;
		exit(0);
	 }
}

void releaseMemory(){
	shmdt(sharedInt);
	shmctl(sharedIntSegmentID, IPC_RMID, NULL);

	shmdt(flags);
	shmctl(flagsSegmentID, IPC_RMID, NULL);

	shmdt(turn);
	shmctl(turnSegmentID, IPC_RMID, NULL);

	shmdt(slaveProcessGroup);
	shmctl(slaveProcessGroupSegmentID, IPC_RMID, NULL);

	shmdt(slaveNum);
	shmctl(slaveSegmentID, IPC_RMID, NULL);

	// delete(file);
	shmdt(fileName);
	shmctl(fileNameSegmentID, IPC_RMID, NULL);

	shmdt(maxWrites);
	shmctl(maxWritesSegmentID, IPC_RMID, NULL);

}