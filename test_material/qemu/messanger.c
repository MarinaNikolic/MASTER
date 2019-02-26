#include <stdio.h>                                                       
#include <signal.h>                                                      
#include <unistd.h>     
#include <stdlib.h>

static int pid;

void message_handler(int signo){
    
    if (signo==SIGUSR1){
	printf("Sending signal to pid %d to dump data\n", pid);
        
	union sigval coverage_code;
        coverage_code.sival_int = 45949;

        sigqueue(pid, SIGUSR1, coverage_code);

    }
}


int main(int argc, char**argv){

	if(argc == 2) {
		pid = atoi(argv[1]);
		printf("Registrating signal SIGUSR1 for passing message 45949 to pid: %d...\n",pid);
		signal(SIGUSR1,message_handler);
		while(1){}
	}
	else{
		printf("Incorrect number of command line arguments. Passing %d instead of 1.\n", argc-1);
		printf("Please pass a single number as argument that presents the PID of target process.\n");
	}
	return 0;
}
