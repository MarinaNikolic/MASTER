#include <stdio.h>                                                       
#include <signal.h>                                                      
#include <unistd.h>     

#define COVERAGE_SIGNAL SIGUSR1
extern void drew_coverage();

void coverage_handler(int signo){
    if (signo==COVERAGE_SIGNAL){
         printf("Dumping coverage data... \n");
	 drew_coverage();
    }
}

__attribute__((constructor))
void coverage_signal_registry(){
    printf("Registrating signal SIGUSR1 for coverage data dump...\n");
    signal(SIGUSR1,coverage_handler);
}

                                                                         
