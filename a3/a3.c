#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string.h>
#include <stdint.h>

#define RESPONSE_PIPE "RESP_PIPE_53306"
#define REQUEST_PIPE "REQ_PIPE_53306"
#define SHM_NAME "/VNZcQM"

typedef union _conv_
{
    u_int8_t bytes[4];
    u_int32_t inv_val;
}number_req;

typedef struct _SECTION_HEADER
{
    char name[17];
    unsigned int type;
    unsigned int offset;
    unsigned int size;
}SECTION_HEADER;

typedef struct _HEADER
{
   unsigned char version;
   unsigned char no_sections;
   SECTION_HEADER *section_headers;
   unsigned short size;
   char magic[5];
}HEADER;

int shm_fd;
void* shm_pointer=NULL;
u_int32_t memory_size;

int file_fd;
char* file_pointer=NULL;


HEADER read_header(int fd,struct stat metadata){
    HEADER header;
    int offset=metadata.st_size-(sizeof(header.size)+sizeof(header.magic)-1);
    unsigned short header_size;
    memcpy(&header_size,file_pointer+offset,sizeof(header.size));

    int header_offset=metadata.st_size-header_size;
    memcpy(&header.version,file_pointer+header_offset,sizeof(header.version));
    header_offset+=sizeof(header.version);
    
    memcpy(&header.no_sections,file_pointer+header_offset,sizeof(header.no_sections));
    header_offset+=sizeof(header.no_sections);

    header.section_headers=malloc(sizeof(SECTION_HEADER)*header.no_sections);

    for(int i=0;i<header.no_sections;i++){
        SECTION_HEADER headers;
        memcpy(&headers.name,file_pointer+header_offset,sizeof(headers.name)-1);
        headers.name[16]=0;
        header_offset+=sizeof(headers.name)-1;

        memcpy(&headers.type,file_pointer+header_offset,sizeof(headers.type));
        header_offset+=sizeof(headers.type);

        memcpy(&headers.offset,file_pointer+header_offset,sizeof(headers.offset));
        header_offset+=sizeof(headers.offset);

        memcpy(&headers.size,file_pointer+header_offset,sizeof(headers.size));
        header_offset+=sizeof(headers.size);
    
        header.section_headers[i]=headers;
    }

    memcpy(&header.size,file_pointer+header_offset,sizeof(header.size));
    header_offset+=sizeof(header.size);

    memcpy(&header.magic,file_pointer+header_offset,sizeof(header.magic)-1);
    header.magic[4]=0;
    
    return header;
}
char* read_string(int req){
    
    char* request=malloc(100);
    int index=0;
    char buff='\0';
    while (buff!='#'){
        read(req,&buff,1);
        request[index++]=buff;
    }
    request[--index]='\0';
    return request;
}

void handle_ping(int resp){
    char ping[5]="PING";
    ping[4]='#';

    uint32_t num=53306;

    char pong[5]="PONG";
    pong[4]='#';

    write(resp,ping,5);
    write(resp,&num,4);
    write(resp,pong,5);
}

void handle_shm_create(int req,int resp){
    
    char ret_msg[11]="CREATE_SHM";
    ret_msg[10]='#';

    char success[8]="SUCCESS";
    success[7]='#';

    char error[6]="ERROR";
    error[5]='#';

    read(req,&memory_size,4);

    if((shm_fd=shm_open(SHM_NAME,O_CREAT|O_RDWR,0664))==-1){
        write(resp,&ret_msg,11);
        write(resp,&error,6);
        return;
    }

    ftruncate(shm_fd,memory_size);
    
    shm_pointer=(char*)mmap(0,memory_size,PROT_READ |
                PROT_WRITE,
                MAP_SHARED,
                shm_fd,0);
    
    write(resp,&ret_msg,11);
    write(resp,&success,8);
}

void handle_shm_write(int req,int resp){
    char ret_msg[13]="WRITE_TO_SHM";
    ret_msg[12]='#';

    char success[8]="SUCCESS";
    success[7]='#';

    char error[6]="ERROR";
    error[5]='#';

    u_int32_t offset;
    u_int32_t value;

    read(req,&offset,4);
    read(req,&value,4);
    
    if(offset+4>memory_size){
        write(resp,&ret_msg,13);
        write(resp,&error,6);
        return;
    }

    memcpy(shm_pointer+offset,&value,4);

    write(resp,&ret_msg,13);
    write(resp,&success,8);
    
}

void handle_map_file(int req,int resp){
    char ret_msg[9]="MAP_FILE";
    ret_msg[8]='#';

    char success[8]="SUCCESS";
    success[7]='#';

    char error[6]="ERROR";
    error[5]='#';

    char* file_name=read_string(req);
    if((file_fd=open(file_name,O_RDONLY))==-1){
        write(resp,&ret_msg,9);
        write(resp,&error,6);
        return;
    }

    struct stat stats;
    fstat(file_fd,&stats);

    file_pointer=mmap(0,stats.st_size,PROT_READ,MAP_PRIVATE,file_fd,0);
    if(file_pointer==MAP_FAILED){
        write(resp,&ret_msg,9);
        write(resp,&error,6);
        return;
    }

    
    write(resp,&ret_msg,9);
    write(resp,&success,8);
}

void handle_file_offset(int req,int resp){
    char ret_msg[22]="READ_FROM_FILE_OFFSET";
    ret_msg[21]='#';

    char success[8]="SUCCESS";
    success[7]='#';

    char error[6]="ERROR";
    error[5]='#';

    if(shm_pointer==NULL || file_pointer==NULL){
        write(resp,&ret_msg,22);
        write(resp,&error,6);
        return;
    }

    u_int32_t offset;
    u_int32_t no_bytes;

    read(req,&offset,4);
    read(req,&no_bytes,4);

    struct stat stats;
    fstat(file_fd,&stats);

    if(offset+no_bytes>stats.st_size){
        write(resp,&ret_msg,22);
        write(resp,&error,6);
        return;
    }

    memcpy(shm_pointer,file_pointer+offset,no_bytes);
    write(resp,&ret_msg,22);
    write(resp,&success,8);

}

void handle_file_section(int req,int resp){
    char ret_msg[23]="READ_FROM_FILE_SECTION";
    ret_msg[22]='#';

    char success[8]="SUCCESS";
    success[7]='#';

    char error[6]="ERROR";
    error[5]='#';

    if(shm_pointer==NULL || file_pointer==NULL){
        write(resp,&ret_msg,23);
        write(resp,&error,6);
        return;
    }

    u_int32_t offset;
    u_int32_t section_no;
    u_int32_t no_bytes;

    read(req,&section_no,4);
    read(req,&offset,4);
    read(req,&no_bytes,4);

    struct stat stats;
    fstat(file_fd,&stats);

    HEADER header=read_header(file_fd,stats);
    printf("HEADER NO SECTIONS:%d\nHEADER OFFSET AT %d: %d\n",header.no_sections,offset,header.section_headers[section_no-1].offset);
    if(section_no>header.no_sections ||
        offset+no_bytes > header.section_headers[section_no-1].offset +
        header.section_headers[section_no-1].size){
        write(resp,&ret_msg,23);
        write(resp,&error,6);
        return;
    }    
    
    memcpy(shm_pointer,file_pointer + header.section_headers[section_no-1].offset+offset,no_bytes);
    
    write(resp,&ret_msg,23);
    write(resp,&success,8);
}

void handle_logical_space(int req,int resp){

    char ret_msg[31]="READ_FROM_LOGICAL_SPACE_OFFSET";
    ret_msg[30]='#';

    char success[8]="SUCCESS";
    success[7]='#';

    char error[6]="ERROR";
    error[5]='#';

    u_int32_t offset;
    u_int32_t no_bytes;

    read(req,&offset,4);
    read(req,&no_bytes,4);
    
    int start_memory_address=offset/5120;
    int offset_in_memory_address=offset%5120;
    struct stat stats;
    fstat(file_fd,&stats);

    HEADER header=read_header(file_fd,stats);

    int curr_block=0;
    int next_block=0;
    for(int i=0;i<header.no_sections;i++){

        next_block=((header.section_headers[i].size)/5120 + 1 + curr_block);
        
        if(start_memory_address>=curr_block && start_memory_address<next_block){
            memcpy(shm_pointer,file_pointer + header.section_headers[i].offset+offset_in_memory_address,no_bytes);
            write(resp,&ret_msg,31);
            write(resp,&success,8);
            return;
        }
        curr_block=next_block;
    }

    write(resp,&ret_msg,25);
    write(resp,&error,6);

    return;
}

void handle_request(char* request,int req,int resp){
    if(strcmp(request,"PING")==0){
        handle_ping(resp);
    }
    else if(strcmp(request,"CREATE_SHM")==0){
        handle_shm_create(req,resp);
    }
    else if(strcmp(request,"WRITE_TO_SHM")==0){
        handle_shm_write(req,resp);
    }
    else if(strcmp(request,"MAP_FILE")==0){
        handle_map_file(req,resp);
    }
    else if(strcmp(request,"READ_FROM_FILE_OFFSET")==0){
        handle_file_offset(req,resp);
    }
    else if(strcmp(request,"READ_FROM_FILE_SECTION")==0){
        handle_file_section(req,resp);
    }
    else if(strcmp(request,"READ_FROM_LOGICAL_SPACE_OFFSET")==0){
        handle_logical_space(req,resp);
    }
    else if(strcmp(request,"EXIT")==0){
        unlink(RESPONSE_PIPE);
        shm_unlink(SHM_NAME);
        close(req);
        close(resp);
        close(shm_fd);
        munmap(shm_pointer,memory_size);
        exit(0);
    }
}

int main(){

    unlink(RESPONSE_PIPE);
    if(mkfifo(RESPONSE_PIPE,0666)==-1){
        perror("ERROR\ncannot create response pipe\n");
    }
    
    int req;
    if((req=open(REQUEST_PIPE,O_RDONLY))==-1){
        perror("ERROR\ncannot open the request pipe\n");
    }
    
    int resp;
    if((resp=open(RESPONSE_PIPE,O_WRONLY))==-1){
        perror("ERROR\ncannot open the response pipe\n");
    }

    char test[6]={'H','E','L','L','O','#'};
    
    int written_bytes=0;
    if((written_bytes=write(resp,test,6))==6){
        printf("SUCCESS\n");
    }
        
    while (1){
       
        char *request=read_string(req);
        handle_request(request,req,resp);
    }
    

    unlink(RESPONSE_PIPE);
       
    

    return 0;
}