#ifndef VERIFICADOR
#define VERIFICADOR




typedef struct{
	int porto;

	int n_threads;

	//1=FIFO;2=priority to static requests;3=priority to dynamic requests
	int pol_escalonamento;

	char scripts_permitidos[101];

	//changes to 1 everytime configurations are changed on file. the server will change it to zero once it updates them.
	int alterada;

} confs;


confs* mpconfs;

sem_t *sem_;


//imprime as confs atuais no ecran
void printConfs(){
	printf("port:%d\n",mpconfs->porto);
	printf("number of threads on pool:%d\n",mpconfs->n_threads);
	printf("scheduling policy:%d\n",mpconfs->pol_escalonamento);
	printf("allowed scripts:%s\n",mpconfs->scripts_permitidos);

}




//obtem as confs a partir do ficheiro
void obtemConfs(int sig){
	FILE *f;
	char buf[21];
	if(sig!=0){
            sem_wait(sem_);
            mpconfs->alterada=1;
    }
	f=fopen("confs.txt","r");



	if(f){

		mpconfs->scripts_permitidos[0]='\0';
		fscanf(f,"%d",&mpconfs->porto);
		fscanf(f,"%d",&mpconfs->n_threads);
		fscanf(f,"%d",&mpconfs->pol_escalonamento);

		while(fscanf(f,"%s",buf)!=EOF){
			strcat(mpconfs->scripts_permitidos,buf);
			strcat(mpconfs->scripts_permitidos," ");
		}
		fclose(f);
		printConfs();
        sem_post(sem_);
        if(sig!=0)
            kill(getppid(),SIGHUP);
	}

	else{
		printf("error reading configs file\n");
		sem_post(sem_);
		exit(1);
	}
}

//termina o processo
void catch_ctrlcProcConfs(int sig)
{
    printf("configs process shutting down...\n");
    exit(0);

}


//function used by the configs management process.
//receives a pointer for the shared memory and another for the semaphore used to prevent simultaneous accesses to it.
void verificadorConfs(confs *mp,sem_t* sem,sem_t *init){

	printf("configs process PID:%d\n",getpid());

	mpconfs=mp;

    sem_=sem;

    signal(SIGINT,catch_ctrlcProcConfs);

    obtemConfs(0);

	signal(SIGHUP,obtemConfs);

    sem_post(init);

	while(1)
		pause();

}


#endif

