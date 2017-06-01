/**
Author: Jeffery Calhoun
Course: CS 4760 Operating Systems Section 2 (TTh 9:30-10:45am)
Date: 10/05/2016
*/

#include <iostream>
#include <unistd.h>
#include <ctime>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>
#include <sys/stat.h>
#include <fstream> //for writing to file
#include <cstdlib>

using namespace std;

enum state { idle, want_in, in_cs };


char* getFormattedTime(); //gets local time
void terminateSigHandler(int); //handles signal from parent to terminate on Ctrl+C
void timeoutSigHandler(int); //handles signla from parent to terminate on timeout

int id; //process id, used by multiple functions

int main(int argc, char ** argv){


	signal(SIGTERM, terminateSigHandler);
	signal(SIGUSR1, timeoutSigHandler);

	if(argc < 2){ //If no arguments were supplied, id must not be set
		perror("No argument supplied for id");
		exit(1);
	}
	else{
		id = atoi(argv[1]);
	}

	srand(time(0)); //generate different delays each run

	int N; //number of slave processes master will spawn

	//Shared memory key, id, and data store used to share number of slaves to spawn
	 int slaveKey = ftok("Makefile", 5);
	 int slaveSegmentID;
	 int *slaveNum;

	 if((slaveSegmentID = shmget(slaveKey, sizeof(int), IPC_CREAT|S_IRUSR | S_IWUSR)) < 0){
	 	perror("shmget: Failed to allocate shared memory for slaveNum");
	 	exit(1);
	 }
	 else{
	 	slaveNum = (int *)shmat(slaveSegmentID, NULL, 0);
	 	N = *slaveNum;
	 }
	
	//Shared memory key, id, and data store used for sharedNum to be incremented
	int sharedIntKey = ftok("Makefile", 1);
	int sharedIntSegmentID;
	int *sharedInt;

	if((sharedIntSegmentID = shmget(sharedIntKey, sizeof(int), IPC_CREAT|S_IRUSR | S_IWUSR)) < 0){
		perror("shmget: Failed to allocate shared memory for shared int");
		exit(1);
	}
	else{
		sharedInt = (int *) shmat(sharedIntSegmentID, NULL, 0);
	}

	//Shared memory key, id, and data store used for flags array in Peterson's algorithm
	int flagsKey = ftok("Makefile", 2); //
	int flagsSegmentID;
	int *flags;

	if((flagsSegmentID = shmget(flagsKey, N * sizeof(int), IPC_CREAT|S_IRUSR | S_IWUSR)) < 0){
		perror("shmget: Failed to allocate shared memory for flags array");
		exit(1);
	}
	else{
	 	flags = (int *)shmat(flagsSegmentID, NULL, 0);
	 }

	 ///Shared memory key, id, and data store used for turn variable in Peterson's algorithm
	 int turnKey = ftok("Makefile", 3);
	 int turnSegmentID;
	 int *turn;

	 if((turnSegmentID = shmget(turnKey, sizeof(int), IPC_CREAT|S_IRUSR | S_IWUSR)) < 0){
	 	perror("shmget: Failed to allocate shared memory for turn array");
	 	exit(1);
	 }
	 else{
	 	turn = (int *)shmat(turnSegmentID, NULL, 0);
	 }


	 //Shared memory key, id, and data store used to share output file name from master to slaves
	int fileNameKey = ftok("Makefile", 6);
	int fileNameSegmentID;
	char *fileName;
	ofstream file;

	if((fileNameSegmentID = shmget(fileNameKey, sizeof(char) * 26, IPC_CREAT|S_IRUSR | S_IWUSR)) < 0){
	 	perror("shmget: Failed to allocate shared memory for ofstream");
	 	exit(1);
	 }
	 else{
	 	fileName = (char *)shmat(fileNameSegmentID, NULL, 0);
	 	
	 	file.open(fileName, ofstream::out | ofstream::app);
	 }
	 

	//Shared memory key, id, and data store used to share number of times slave should enter crit. sec.
	int maxWritesKey = ftok("Makefile", 7);
	int maxWritesSegmentID;
	int *maxWrites;

	if((maxWritesSegmentID = shmget(maxWritesKey, sizeof(int), IPC_CREAT|S_IRUSR | S_IWUSR)) < 0){
	 	perror("shmget: Failed to allocate shared memory for maxWrites");
	 	exit(1);
	 }
	 else{
	 	maxWrites = (int *)shmat(maxWritesSegmentID, NULL, 0);
	 	
	 }


	cerr << getFormattedTime() << ": Process " << id << " wants to enter critical section\n";

	int j;
	for (int i = 0; i < *maxWrites; i++ ){
		
		//execute code to enter critical section;
		do{
			flags[id - 1] = want_in;
			j = *turn;

			while(j != id - 1){
				j = (flags[j] != idle) ? *turn : (j + 1) % N ;
			}

				flags[id - 1] = in_cs;

				//check that no-one else is in critical section
				for(j = 0; j < N; j++){
					if((j != id - 1) && (flags[j] == in_cs)){
						break;
					}
				}

		}while( (j < N) || ((*turn != id-1) && (flags[*turn] != idle)) );
		*turn = id-1;

		/* Critical section */

		cerr << getFormattedTime() << ": Process " << id << " in critical section\n";

		//sleep for random amount of time (between 0 and 2 seconds);
		sleep(rand() % 3);

		//increment sharedNum
		++(*sharedInt);

		//write message into the file
		file << "File modified by process number " << id << " at time " << getFormattedTime() << " with sharedNum = " << (*sharedInt) << endl;
		
		//sleep for random amount of time (between 0 and 2 seconds);
		sleep(rand() % 3);

		//exit from critical section;


		cerr << getFormattedTime() << ": Process " << id << " exiting critical section\n";

		j = (*turn + 1) % N;

		while(flags[j] == idle){
			j = (j + 1) % N;
		}

		*turn = j;

		flags[id-1] = idle;
		kill(getppid(), SIGUSR2);
		
	}

	return 0;
}


char* getFormattedTime(){
	int timeStringLength;
	string timeFormat;

	timeStringLength = 9;
	timeFormat = "%H:%M:%S";

	time_t seconds = time(0);

	struct tm * ltime = localtime(&seconds);
	char *timeString = new char[timeStringLength];

	strftime(timeString, timeStringLength, timeFormat.c_str(), ltime);

	return timeString;
}

void terminateSigHandler(int signal){
	if(signal == SIGTERM){
		cerr << "Process " << id << " exiting due to interrupt signal.\n";
		exit(1);
	}
}

void timeoutSigHandler(int signal){
	if(signal == SIGUSR1){
		cerr << "Process " << id << " exiting due to timeout.\n";
		exit(0);
	}
}