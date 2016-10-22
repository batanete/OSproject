//este programa "dorme" o numero de segundos indicado pelo utilizador

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>


int main(int argc,char** argv){
	int tDormir;
	
	if(argc!=2){
		printf("por favor indique apenas um argumento\n");
		return 0;
	}
	tDormir=atoi(argv[1]);

	sleep(tDormir);

	printf("dormi %d segundos\n",tDormir);

	return 0;
}
