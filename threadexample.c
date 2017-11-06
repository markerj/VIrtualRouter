/*
 * Lab 4
 * Tyler Paquet
 * Anthony Dowling
 * Due: 2/9/2017
*/
#include <pthread.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>

#define MAXLINE 200

void *work(void *arg);
void exithandler(int sig);

char file[MAXLINE];
int openthreads = 0;
int numofrequests = 0;
int numserviced = 0;

int main()
{

	printf("Enter a filename to search for. Enter as many as you would like. Ctrl-C to exit.\n");
	while(1)
	{
		signal(SIGINT, exithandler);    
		fgets(file, MAXLINE, stdin);
		
		//for full speed effect, allocate memory in heap and copy string address. Then pass that address to new thread function
		
		pthread_t worker;
		int status;

		if ((status = pthread_create(&worker, NULL, work, &file)) != 0)
		{
			fprintf(stderr, "thread create error %d: %s\n", status, strerror(status));
			exit(1);
		}

		openthreads++;
		numofrequests++;
	}
	return 0;

}

void *work(void *arg)
{
	char localcopy[MAXLINE];
	strcpy(localcopy, arg);		//still could cause problems at full speed
	srand(time(NULL));
	int prob = (rand() % 10) + 1;

	if(prob == 1 || prob == 2)
	{
		int prob2 = (rand() % 4) + 7;
		sleep(prob2);
		printf("File found! %s", localcopy);
	}

	else
	{
		sleep(1);
		printf("File found! %s", localcopy);
	}
	
	openthreads--;
	numserviced++;
	return NULL;
}

void exithandler(int sig)
{
	printf("\nCtrl-C received\n");
	while(openthreads > 0)
	{
	}
	printf("Number of file requests: %d\n", numofrequests);
	printf("Number of requests received: %d\n", numserviced);
	printf("Shutting down... Goodbye!\n");
	exit(0);
}
