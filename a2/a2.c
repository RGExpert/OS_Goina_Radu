#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <semaphore.h>
#include "a2_helper.h"
#include <fcntl.h>
#include <pthread.h>

sem_t  P4_3,P4_1;

sem_t P6,P6_main,P6_t11;
pthread_cond_t cond6=PTHREAD_COND_INITIALIZER;
pthread_cond_t cond6_atleast6_threads=PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex6=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex6_threads=PTHREAD_MUTEX_INITIALIZER;

int threads_active=0;
int flag=0;
int th11_in_execution=0;
int th11_was_executed=0;

sem_t *P7_44,*P7_75;


void *P4_th_function(void* args){
    int i=*(int*)args;
    
    if(i==3){
        info(BEGIN,4,i);
        sem_post(&P4_1);
        sem_wait(&P4_3);
        info(END,4,i);
    }
    else if(i==1){
        
        sem_wait(&P4_1);
        info(BEGIN,4,i);
        info(END,4,i);
        sem_post(&P4_3);
     
    }
    else if(i==4){
        P7_44=sem_open("/sem44",0);
        printf("Waiting\n");
        sem_wait(P7_44);
        info(BEGIN,4,i);
        info(END,4,i);
        sem_close(P7_44);
        
        sem_post(P7_75);
    }
    else{
        info(BEGIN,4,i);
        info(END,4,i);
    }
    return NULL;
}

void* P7_th_function(void* args){
    
    int id=*((int*)args);
    
    if(id==4){
        P7_75=sem_open("/sem75",0);
        sem_wait(P7_75);
        sem_close(P7_75);
    }
    info(BEGIN,7,id);
    
    info(END,7,id);
    if(id==5){
        P7_44=sem_open("/sem44",0);
        printf("Send message\n");
        sem_post(P7_44);
        sem_close(P7_44);
    }
    return NULL;
}

void* P6_th_function(void* args){
    
    int id=*((int*)args);
    
    sem_wait(&P6);
    info(BEGIN,6,id);
    
    pthread_mutex_lock(&mutex6);
    threads_active++;

    if(id==11){
        th11_in_execution=1;
    }

     if(threads_active==6){
        if(!th11_in_execution){ 
            pthread_cond_signal(&cond6_atleast6_threads);
            pthread_cond_signal(&cond6_atleast6_threads);
            pthread_cond_signal(&cond6_atleast6_threads);
        }
        else{                  
            pthread_cond_broadcast(&cond6_atleast6_threads);
        }
    }
    pthread_mutex_unlock(&mutex6);

    pthread_mutex_lock(&mutex6_threads);
    if(!th11_was_executed && id!=11){
        pthread_cond_wait(&cond6_atleast6_threads,&mutex6_threads);
    }
    pthread_mutex_unlock(&mutex6_threads);
    
    
    if(id!=11 && th11_in_execution==1){
        sem_post(&P6_t11);  
        
        sem_wait(&P6_main); 
        sem_post(&P6_main);
        
    }
    
    if(id==11){
        sem_wait(&P6_t11); 
        th11_was_executed=1;
        pthread_cond_broadcast(&cond6_atleast6_threads);
    }

    pthread_mutex_lock(&mutex6);
    threads_active--;
    pthread_mutex_unlock(&mutex6);
    
    info(END,6,id);
    
    if(id==11)
        sem_post(&P6_main); 
    
    sem_post(&P6);
    return NULL;
}

int main(){
    init();

    int pid2,pid3,pid4,pid5,pid6,pid7;
    info(BEGIN, 1, 0);
    pid2=fork();
    P7_44=sem_open("/sem44",O_CREAT,0666,0);
    P7_75=sem_open("/sem75",O_CREAT,0666,0);
    if(pid2==0){    //process 2    
        
        info(BEGIN,2,0);
        pid3=fork();
        if(pid3==0){    //process 3
            info(BEGIN,3,0);
            
            info(END,3,0);
            exit(0);
        }
        wait(NULL);
        
        info(END,2,0);
        exit(0);
    }
    else if(pid2>0){    //process 1
        
        pid4=fork();
        if(pid4==0){    //process 4
            info(BEGIN,4,0);
            
            pid5=fork();
            if(pid5==0){ //process 5
                info(BEGIN,5,0);
                
                pid6=fork();
                if(pid6==0){ //process 6
                    info(BEGIN,6,0);
        
                    sem_init(&P6,0,6);
                    sem_init(&P6_main,0,0);
                    sem_init(&P6_t11,0,0);

                    pthread_t P6_threads[39];
                    int args[39];
                    for(int i=0;i<39;i++){
                        args[i]=i+1;
                        pthread_create(&P6_threads[i],NULL,P6_th_function,&args[i]);
                    }

                    for(int i=0;i<39;i++)
                        pthread_join(P6_threads[i],NULL);

                    info(END,6,0);
                    exit(0);
                    sem_destroy(&P6);
                    sem_destroy(&P6_main);
                    sem_destroy(&P6_t11);
                }
                wait(NULL);
                info(END,5,0);
                exit(0);        
            }
            wait(NULL);

            sem_init(&P4_3,0,0);
            sem_init(&P4_1,0,0);
            
            pthread_t P4_threads[5];
            int P4_args[5];
            for(int i=0;i<5;i++){
                P4_args[i]=i+1;
                pthread_create(&P4_threads[i],NULL,P4_th_function,&P4_args[i]);
            }

            for(int i=0;i<5;i++){
                pthread_join(P4_threads[i],NULL);
            }
            sem_destroy(&P4_3);
            sem_destroy(&P4_1);

            info(END,4,0);
            exit(0);
        }
        else {  //process 1
            pid7=fork();
            if(pid7==0){ //process 7
                info(BEGIN,7,0);
                pthread_t P7_threads[6];
                int P7_args[5];
                for(int i=0;i<6;i++){
                    P7_args[i]=i+1;
                    pthread_create(&P7_threads[i],NULL,P7_th_function,&P7_args[i]);
                }
                for(int i=0;i<6;i++){
                    pthread_join(P7_threads[i],NULL);
                }
        
                info(END,7,0);
                exit(0);
            }
        }
    }   
    wait(NULL);
    wait(NULL);
    wait(NULL);
    sem_unlink("/sem44");
    sem_unlink("/sem75");
    info(END, 1, 0);
    return 0;
}
