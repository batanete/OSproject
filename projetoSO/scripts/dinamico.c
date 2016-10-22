#include <stdio.h>
#include <stdlib.h>


int main(int argc,char* argv[]){
	int i;
	fflush(stdout);
	printf("parte dinamica feita\n");
	printf("argumentos passados pelo utilizador:\n");
	for(i=0;i<argc;i++){
		printf("%s\n",argv[i]);
	}
	
	exit(0);
}
