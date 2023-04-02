#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define TRUE 1
#define FALSE 0

typedef struct _SECTION_HEADER
{
    char name[17];
    int type;
    int offset;
    int size;
}SECTION_HEADER;

typedef struct _HEADER
{
   unsigned char version;
   char no_sections;
   SECTION_HEADER *section_headers;
   short size;
   char magic[4];
}HEADER;

HEADER read_header(int fd,struct stat metadata){
    
    HEADER header;
    unsigned char size[2];
    int offset=metadata.st_size-(sizeof(header.size)+sizeof(header.magic));
    
    lseek(fd,offset,SEEK_SET);
    read(fd,size,sizeof(header.size));
    int header_size=offset=size[0]+(size[1]<<8);
    
    lseek(fd,0,SEEK_SET);
    lseek(fd,metadata.st_size-header_size,SEEK_SET);
    
    read(fd,&header.version,sizeof(header.version));
    read(fd,&header.no_sections,sizeof(header.no_sections));
    
    header.section_headers=malloc(sizeof(SECTION_HEADER)*header.no_sections);
    

    for(int i=0;i<header.no_sections;i++){
        
        SECTION_HEADER headers;
        read(fd, &headers.name, sizeof(headers.name)-1);
        headers.name[16]=0;
        read(fd, &headers.type, sizeof(headers.type));
        read(fd, &headers.offset, sizeof(headers.offset));
        read(fd, &headers.size, sizeof(headers.size));

        header.section_headers[i]=headers;
    }

    read(fd,&header.size,sizeof(header.size));
    read(fd,&header.magic,sizeof(header.magic));

    return header;
}

int* test_header(HEADER header){
    int *error_flags=malloc(5);
    memset(error_flags,0,5);

    if(strcmp(header.magic,"TNnf")!=0){
        error_flags[0]=TRUE;
        error_flags[1]=TRUE;
    }
    if(header.version>149 || header.version<66){
        error_flags[0]=TRUE;
        error_flags[2]=TRUE;
    }
    if(header.no_sections<3 || header.no_sections>16){
        error_flags[0]=TRUE;
        error_flags[3]=TRUE;
    }
    int valid_set[5]={25,38,55,15,32};

    for(int i=0;i<header.no_sections;i++){
        int found=FALSE;
        for(int d=0;d<5;d++){
            if(header.section_headers[i].type==valid_set[d]){
                found=TRUE;    
                break;
            }
        }
        if(found==FALSE){
            error_flags[0]=TRUE;
            error_flags[4]=TRUE;
            break;
        }
    }
    return error_flags;
}

int parse(char* path){

    int fd=open(path,O_RDONLY);
    if(fd==-1){
        perror("ERROR\nCould not open file\n");
        exit(1);
    }

    struct stat metadata;
    fstat(fd,&metadata);

    HEADER header=read_header(fd,metadata);    
    int *error_flags=test_header(header);

    if(error_flags[0]==FALSE){
        printf("SUCCESS\n");
        printf("version=%d\n",header.version);
        printf("nr_sections=%d\n",header.no_sections);
        for(int i=0;i<header.no_sections;i++){
            printf("section%d: %s %d %d\n",i+1,header.section_headers[i].name,header.section_headers[i].type,header.section_headers[i].size);
        }
    }
    else{
        printf("ERROR\nwrong ");
        if(error_flags[1])
            printf("magic");
        else if(error_flags[2])
            printf("version");
        else if(error_flags[3])
            printf("sect_nr");
        else if(error_flags[4])
            printf("sect_types");
        printf("\n");
    }

    free(header.section_headers);
    return 0;
}

int read_line(int fd,char* line,int max_lenght,int *line_lenght,char buff){
    
    line[(*line_lenght)++]=buff;
    int rd;
    while((rd=read(fd,&buff,1)!=0) && *line_lenght<max_lenght && buff!=0x0A){ 
        if(rd==-1)
            return -1;
        else
        line[(*line_lenght)++]=buff;
    }
    line[(*line_lenght)++]='\n';
    line[(*line_lenght)]=0;
    return 0;
}

int get_line(int fd, char* line, int line_no, int max_lenght,int *line_lenght,int offset){

    char buff;
    *line_lenght=0;
    int curr_line=1;
    int rd;
    lseek(fd,offset,SEEK_SET);
    while((rd=read(fd,&buff,1)==1)){
        if(curr_line==line_no){
            return read_line(fd,line,max_lenght,line_lenght,buff);
        }
        if(buff==0x0A) 
          curr_line++;    
    }
    return -1;
}

int get_number_of_lines(int fd,int offset,int max_read){

    int line_number=1;
    char *buff=malloc(max_read);
    
    lseek(fd,0,SEEK_SET);
    lseek(fd,offset,SEEK_SET);
    
    read(fd,buff,max_read);
    
    for(int i=0;i<=max_read;i++){
        if(buff[i]==0x0A)
            line_number++;

        if(line_number>14)
            break;
    }
    free(buff);
    return line_number==14;
}



int extract(char* path,int section_no,int line_no){    
    int fd=open(path,O_RDONLY);
    
    if(fd==-1){
        perror("ERROR\ninvalid file\n");
        return -1;
    }

    struct stat metadata;
    fstat(fd,&metadata);

    HEADER header=read_header(fd,metadata);

    if(section_no>header.no_sections){
        perror("ERROR\n invalid section\n");
        return -1;
    }

    char *line=malloc(header.section_headers[section_no-1].size);
    int line_lenght=0;

    if(get_line(fd,line,line_no,header.section_headers[section_no-1].size,&line_lenght,header.section_headers[section_no-1].offset)==-1){
        perror("ERROR\n invalid line\n");
        return -1;
    }

    printf("SUCCESS\n%s\n",line);
    free(line);
    close(fd);
    return 1;
}


int ends_with(char* str,char* src_string){
    
    int len_str=strlen(str);
    int len_src=strlen(src_string);
    if(len_str<len_src)
        return 0;
    else return strcmp(str+len_str-len_src,src_string)==0;
}


int list(int recursive,int perm_write,char* src_string,char* path){
    
    DIR* dir=opendir(path);

    if(dir==NULL){
        return -1;
    }

    char name[260];
    
    struct dirent *entry;
    struct stat inode;
    

    while((entry=readdir(dir))){
        snprintf(name,sizeof(name),"%s/%s",path,entry->d_name);
        if(strcmp(entry->d_name,".")!=0 && strcmp(entry->d_name,"..")!=0){
            stat(name,&inode);
            if(S_ISDIR(inode.st_mode)){
                
                if(recursive){
                    list(recursive,perm_write,src_string,name);
                }
                if((perm_write==1 && (S_IWUSR & inode.st_mode)) || perm_write==0){

                    if(src_string==NULL || (src_string!=NULL && ends_with(name,src_string))){
                        printf("%s\n",name);
                    }
                }
            }
            else if(S_ISREG(inode.st_mode)){
                 if((perm_write==1 && (S_IWUSR & inode.st_mode)) || perm_write==0){
                     
                     if(src_string==NULL || (src_string!=NULL && ends_with(name,src_string))){
                        printf("%s\n",name);
                    }
                }
            }
        }
    }
    closedir(dir);
    return 0;
}

int findall(char* path,int first_rec){
    DIR* dir=opendir(path);

    if(dir==NULL){
        if(first_rec)
            perror("ERROR\ninvalid directory path\n");
        return -1;
    }

    char name[260];
    
    struct dirent *entry;
    struct stat inode;
    
    if(first_rec){
        printf("SUCCESS\n");
        first_rec=FALSE;          
    }
    
    while((entry=readdir(dir))){
        snprintf(name,sizeof(name),"%s/%s",path,entry->d_name);
        if(strcmp(entry->d_name,".")!=0 && strcmp(entry->d_name,"..")!=0){
            stat(name,&inode);
            if(S_ISREG(inode.st_mode)){
                int fd;
                fd=open(name,O_RDONLY);
               
                struct stat metadata;
                fstat(fd,&metadata);

                HEADER header=read_header(fd,metadata);
                int *error_flags=test_header(header);
                if(!error_flags[0]){
                    int found=FALSE;

                    for(int i=0;i<header.no_sections && !found;i++){
                        if(get_number_of_lines(fd,header.section_headers[i].offset,header.section_headers[i].size))
                                found=TRUE;
                    }
                    if(found){
                        printf("%s\n",name);
                    }
                }
                close(fd);
                free(error_flags);
            }
        }
    }
    rewinddir(dir);
    while((entry=readdir(dir))){
        snprintf(name,sizeof(name),"%s/%s",path,entry->d_name);
        if(strcmp(entry->d_name,".")!=0 && strcmp(entry->d_name,"..")!=0){
        stat(name,&inode);
            if(S_ISDIR(inode.st_mode)){
                findall(name,FALSE);
                }
            }
        }
    closedir(dir);
    return 0;
}

int main(int argc, char **argv){
    if(argc >= 2){
        if(strcmp(argv[1], "variant") == 0){
            printf("53306\n");
        }
        else if(strcmp(argv[1], "list")==0){
            
            int recursive=FALSE;
            int perm_write=FALSE;
            char src_string[20];
            char path[200];
            for(int i=2;i<argc;i++){
                if(strcmp(argv[i],"recursive")==0)
                    recursive=TRUE;
                else if(strcmp(argv[i],"has_perm_write")==0){
                    perm_write=TRUE;
                }
                else if(strncmp(argv[i],"name_ends_with=",15)==0){
                    strcpy(src_string,argv[i]+15);
                }
                else if(strncmp(argv[i],"path=",5)==0){
                    strcpy(path, argv[i]+5);
                }
                else{
                    perror("ERROR\ninvalid arguments\n");
                    exit(1);
                }
                
            }
                printf("SUCCESS\n");
                list(recursive,perm_write,src_string,path);
        }
        else if(strcmp(argv[1], "parse") == 0){
            char path[300];
            if(strncmp(argv[2],"path=",5)==0)
                strcpy(path,argv[2]+5);
        
            parse(path);
        }
        else if(strcmp(argv[1],"extract")==0){
            char path[300];
            int section;
            int line_no;
            if(argc<5){
                perror("ERROR\ninvalid argumets\n");
                return -1;
            }
            strcpy(path,argv[2]+5);
            section=atoi(argv[3]+8);
            line_no=atoi(argv[4]+5);

            extract(path,section,line_no);
        }
        else if(strcmp(argv[1],"findall")==0){
            char path[300];
            if(argc<3){
                perror("ERROR\ninvalid argumets");
                return -1;
            }
            strcpy(path,argv[2]+5);
            findall(path,TRUE);
        }
    }
    return 0;
}