#ifndef REQDIN
#define REQDIN


//verifica se o ficheiro indicado existe
int ficheiroExiste(char *path){
	FILE *file;
    	if ((file = fopen(path, "r"))!=NULL)
    	{
        	fclose(file);
        	return 1;
    	}
    	return 0;
}

//obtem os argumentos a passar ao script
char **obtemArgumentos(char* nomeScript){
	char** res=(char**)malloc(11*sizeof(char*));
	int i=0;
	char delims[2]=";";

	char* tok=strtok(nomeScript,delims);

	while((tok!=NULL)&&(i<10)){
		res[i]=tok;
		tok=strtok(NULL,delims);

		i++;
	}
	for(;i<10;i++)
		res[i]=NULL;
    res[10]=NULL;

	return res;
}

//devolve os conteudos de um ficheiro num buffer
void leFich(char *nomeFich,char *buf){
	FILE *f;
	long fsize;
	if ((f = fopen(nomeFich, "r"))!=NULL)
    	{
        	fseek(f,0,SEEK_END);
		fsize=ftell(f);
		fseek(f,0,SEEK_SET);
		fread(buf,fsize,1,f);
		fclose(f);
		buf[fsize]='\0';
    	}


}


//executa um request dinamico,caso o ficheiro exista
char *reqDinamico(char *nomeScript){
	char *path=(char*) malloc(20*sizeof(char));
	char *res=(char*)malloc(1001*sizeof(char));
    path[0]='\0';
	char **args=obtemArgumentos(nomeScript);
	char extensao[4];

	char buf[1001];


	int sizeRes;

	sprintf(extensao,"%s",nomeScript+strlen(nomeScript)-3);
	int fd[2];
	pid_t pidFilho;
	pipe(fd);
	path[0]='\0';
	res[0]='\0';

	strcat(path,"scripts/");
	strcat(path,nomeScript);
	if(!ficheiroExiste(path)){
		free(path);
		return NULL;
	}
	pidFilho=fork();
	if(pidFilho==0){
		dup2(fd[1],fileno(stdout));
		close(fd[0]);
		close(fd[1]);
		if(strcmp(extensao,".sh")==0){
			leFich(path,buf);
			system(buf);
		}
		else{
            execv(path,args);
        }
            exit(0);
	}
	else if(pidFilho!=-1){
		sizeRes=read(fd[0],res,1001*sizeof(char));
		res[sizeRes]='\0';
		close(fd[0]);
		close(fd[1]);
		waitpid(pidFilho,NULL,0);
	}
	else{
		printf("ERRO NO FORK DE UM REQUEST DINAMICO\n");
		exit(1);
	}
	free(path);
	return res;
}


#endif
