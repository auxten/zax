#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include "zax-multi-thread.h"
#include "version.h"


static unsigned char *switch_read_buf;
static unsigned char *window_next_p;
static int read_num;
static int byte_remain=0;
static int extra_byte=0;
static char * instream_buf,* outstream_buf;
static unsigned char *decode_buf;

static void errquit(const char *p) {
    printf("%s\n",p);
    system("PAUSE");
    exit (1);
}
static int init_index() {
    memset(hash_index,-1,65536*sizeof(CHAIN));//256*256*12
    return 0;
}
void * indexWorker(void * rng) {
//for malloc efficiency!!!!
    int tmp_p;//temp position
    unsigned char i,j;
    CHAIN *index_p,*lck_index;
    CHAIN *new_node, *old_next;
    unsigned int * rnge;

    rnge = (unsigned int *)rng;
    for (tmp_p=*rnge;tmp_p < *(rnge+1);tmp_p++) {
        i=(unsigned char)(*(file_buf+tmp_p));
        j=(unsigned char)(*(file_buf+tmp_p+1));
        //	printf("#i=%u,j=%u#",i,j);
        index_p = &hash_index[i][j];
        //block until lock the mutex
        lck_index = index_p;
        new_node = malloc(sizeof(CHAIN));
        pthread_mutex_lock (&(lck_index->lock_chain));
        if (index_p->position == -1) {
            index_p->position = tmp_p;
            free(new_node);
            //if the chain head position bigger than tmp_p;do sth please!:p
        } else if (index_p->position > tmp_p) {
            new_node->next = index_p->next;
            new_node->position = index_p->position;
            index_p->position = tmp_p;
            index_p->next = new_node;
        } else {
            while (1) {
                if (index_p->next == INITIALIZE)
                    break;
                else
                    if (index_p->next->position > tmp_p) break;
                index_p=index_p->next;
            }
            //insert new position
            //NULL -> INITIALIZE
            old_next = index_p->next;
            index_p->next=new_node;
            new_node->next = old_next;
            new_node->position=tmp_p;
        }
        //unlock the hash_index[i][j].lock_chain
        pthread_mutex_unlock (&(lck_index->lock_chain));
    }
    pthread_exit((void *)0);
    //printf("\n mk_index success\n");
//for malloc efficiency!!!!
}
static int makeindex (unsigned char * file_buff,int size) {

    unsigned char i;
    unsigned int range[THREADS_NUM][2];

    pthread_t worker[THREADS_NUM];
    void *status; // pthread status

    file_buf = file_buff;
//multithread division
    for (i=0;i != THREADS_NUM;i++) {
        range[i][0] = size/THREADS_NUM*i;
        range[i][1] = size/THREADS_NUM*(i+1);
    }
    range[THREADS_NUM-1][1] = size - 1;//last thread must make the work done
    //if size == 13 THREADS_NUM == 4;range will be 0~3 3~6 6~9 9~12
//multithread division
    printf("1:%d~%d,2:%d~%d\n",range[0][0],range[0][1],range[1][0],range[1][1]);
    for (i=0;i<THREADS_NUM;i++) {
        pthread_create(&worker[i], &attr, indexWorker,(void *)range[i]);
    }
    // pthread_attr_* moved to main();

    for (i=0;i<THREADS_NUM;i++) {
        if (0 == pthread_join(worker[i], &status))
            printf ("%d ",i);
    }
    return (0);
}
static int freeindex() {
    int ii,jj;
    CHAIN * index;
    CHAIN * index_free;
    for (ii=0;ii!=256;ii++)
        for (jj=0;jj!=256;jj++) {
            if ((index = (hash_index[ii][jj]).next) == INITIALIZE)
                continue;
            while (index->next != INITIALIZE) {
                index_free = index;
                index = index->next;
                free (index_free);
            }
            //remember to free the last index node!!
            free(index);
        }
    //reset the head!
    memset(hash_index,-1,65536*sizeof(CHAIN));//256*256*8
    return(0);
}
static CODE * genrawcode(void) {
    int code_type=1,chain_counter=0;//repeat_times max is 128
    int best_position = -1,tmp_best_position = -1;
    int tmp_match_num=0,match_num=0,repeat_times=1,offset=0;
    unsigned char i,j,nextchar;
    CODE * tmp_codep;
    CHAIN * matched_index_p;

    //printf("begin genrawcode\n");
    //initial raw_code chain
    raw_code=(CODE *)malloc(sizeof(CODE));
    raw_code->next=INITIALIZE;
    tmp_codep = raw_code;
    //when code chain is MAX_CODE_CHAIN_LEN return the chain
    while ((chain_counter < MAX_CODE_CHAIN_LEN)&&(window_back < file_read_buf1+read_num-1)) {
        i=(unsigned char)(*(window_back+1));
        j=(unsigned char)(*(window_back+2));
        matched_index_p = &hash_index[i][j];

        if ((hash_index[i][j]).next == INITIALIZE) { //no match"only match itself"
            code_type=1;
            repeat_times=1;
        } else { //match at least 2 char
            if ((*(window_back+1) == *(window_back+3)) &&((*(window_back+2) == *(window_back+4)))) {//if found "abab"(1~2 byte repeat)
                //repeat_times-1 max can be 2^7-1=127
                //printf("abab style\n");
                //printf("%d\n",repeat_times);
                code_type=1;
                repeat_times=1;
                //find ababababab or aaaaaaaaa type repeat
                while ((*(window_back + 1) == *(window_back+2*repeat_times+1))\
                        &&(*(window_back + 2) == *(window_back+2*repeat_times + 2))\
                        &&(repeat_times<128)\
                        &&(window_back+2*repeat_times + 2 <= file_read_buf1+read_num-1)) {
                    repeat_times++;
                }
            } else {//if found no 1~2 byte repeat

                tmp_match_num=match_num=2;
                code_type=0;
                //if matched  index is before the window then move to the next index of the chain
                //here,it also can be "matched_index_p->position < (window_back-file_read_buf1-WINSIZE+1)"
                while (matched_index_p->position+file_read_buf1 <= window_back-WINSIZE) {
                    matched_index_p = matched_index_p->next;
                }
                //find continuous matched byte in the window
                while (matched_index_p->position <= window_back-file_read_buf1-1) {
                    tmp_best_position = matched_index_p->position;
                    matched_index_p = matched_index_p->next;
                    tmp_match_num=2;
                    while (*(file_read_buf1+tmp_best_position+tmp_match_num) == *(window_back+tmp_match_num+1)) {
                        if ((file_read_buf1+tmp_best_position+tmp_match_num < window_back)\
                                &&(window_back+tmp_match_num+1 <= file_read_buf1+read_num-1)\
                                &&(tmp_match_num < 31))//only 5bits for match_num
                            tmp_match_num++;
                        else
                            break;
                    }
                    if (tmp_match_num>match_num) {
                        match_num=tmp_match_num;
                        best_position=tmp_best_position;

                    }
                    //match_num=(tmp_match_num>match_num)?tmp_match_num:match_num;
                    //best_position=(tmp_match_num>match_num)?tmp_best_position:best_position;
                }
                if (match_num == 2)
                    code_type = 1;
                repeat_times = 1;
            }
        }
        //encode raw_code by types
        //printf("code_type = %d\n",code_type);
        switch (code_type) {
        case 0: //code first bit is 0
            offset = (file_read_buf1+best_position-(window_back-(WINSIZE-1)));
            nextchar = *(window_back+match_num+1);
            tmp_codep->code[0] = offset>>3;
            tmp_codep->code[1] = (unsigned char)((offset&7)<<5) + (unsigned char)match_num;
            tmp_codep->code[2] = nextchar;

            //init next CODE
            tmp_codep->next = (CODE *)malloc(sizeof(CODE));
            tmp_codep = tmp_codep->next;
            tmp_codep->next=NULL;
            window_back += match_num+1;
            chain_counter++;
            //check if the nextchar is extra!
            if (window_back == file_read_buf1 + read_num)
                extra_byte = 1;
            break;

        case 1: //code first bit is 1
            tmp_codep->code[0] = 128+repeat_times-1;
            tmp_codep->code[1] = i;
            tmp_codep->code[2] = j;
            //init next CODE
            tmp_codep->next = (CODE *)malloc(sizeof(CODE));
            tmp_codep = tmp_codep->next;
            tmp_codep->next=NULL;
            window_back += 2*repeat_times;
            chain_counter++;
            break;
        }

        if (file_read_buf1+read_num-window_back -1 <= MAX_DECODE_LEN) {//window move to the may-over-code area
            if (read_num == FILE_BUF_SIZE1)// last read full fill the buffer
                break;//read new data to filebuffer

            else {// last read didn't full fill the buffer
                //2 bytes left
                if (file_read_buf1+read_num-window_back-1 == 2) {
                    tmp_codep->code[0] = 128;
                    tmp_codep->code[1] = (unsigned char)(*(window_back+1));
                    tmp_codep->code[2] = (unsigned char)(*(window_back+2));
                    tmp_codep->next = (CODE *)malloc(sizeof(CODE));
                    tmp_codep = tmp_codep->next;
                    tmp_codep->next=NULL;
                    window_back = file_read_buf1+read_num-1;
                    chain_counter++;
                    break;
                } else {//1 byte left
                    if (file_read_buf1+read_num-window_back-1 == 1) {
                        tmp_codep->code[0] = 0;
                        tmp_codep->code[1] = 0;
                        tmp_codep->code[2] = (unsigned char)(*(window_back+1));
                        tmp_codep->next = (CODE *)malloc(sizeof(CODE));
                        tmp_codep = tmp_codep->next;
                        tmp_codep->next=NULL;
                        window_back = file_read_buf1+read_num-1;
                        chain_counter++;
                        break;
                    }
                }
            }
        }
    }
    byte_remain = file_read_buf1+read_num-(window_back-(WINSIZE-1));
    return (raw_code);
}
static int compress(FILE const* infd,FILE const* outfd,const int mode) {

    int read_counter=0;
    int code80_counter=0;
    CODE * code80_p=NULL;
    int remain80;

    CODE * raw_code_free;
    file_read_buf1 = (unsigned char *)calloc(FILE_BUF_SIZE1,sizeof(char));
    file_read_buf2 = (unsigned char *)calloc(FILE_BUF_SIZE2,sizeof(char));
    window_back = file_read_buf1 - 1;

    setbuf(infd,instream_buf);
    setbuf(outfd,outstream_buf);
    init_index();
    while ((read_num = fread(file_read_buf2+byte_remain,1,FILE_BUF_SIZE1-byte_remain,infd) + byte_remain) > WINSIZE || read_counter == 0) {
        if (byte_remain != 0) {
            memcpy(file_read_buf2,(window_back-(WINSIZE-1)),byte_remain);
        }
        //for efficiency here i switch the pointer of buf instead of memcpy the
        //larger block of memory
        switch_read_buf = file_read_buf1;
        file_read_buf1 = file_read_buf2;
        file_read_buf2 = switch_read_buf;
        //pay attention to update the window_back pointer!!!
        window_back=(read_counter++?(WINSIZE-1+file_read_buf1):(file_read_buf1 - 1));

        //	for(i=0;i<1000;i++)
        //		{printf ("%u\n",file_read_buf1[i]);}
        makeindex(file_read_buf1,read_num);
        //	printf("%u\n",hash_index[102][111]->next->next->position);
        if (genrawcode() == NULL)
            errquit("\n genrawcode error! \n");
        //printf("%d gencode over!\n",read_counter);
        while (raw_code->next != NULL) {
            //recode the rawcode to optimize the continuous "80" code
            if ((raw_code->code[0] == 128) && !(code80_counter--)) {
                code80_counter = 1;
                code80_p = raw_code->next;
                while ((code80_p->next != NULL) && (code80_counter < MAX_80_NUM) && (code80_p->code[0] == 128)) {
                    code80_counter++;
                    code80_p = code80_p->next;
                }
                if (code80_counter < 4) {
                    continue;
                } else {
                    fputc(code80_counter*2>>3,outfd);
                    fputc((unsigned char)((code80_counter*2&7)<<5),outfd);
                    fputc(0,outfd);
                    remain80 = (3 - (2 * code80_counter)%3)%3;
                    //code80_p = raw_code;
                    for (;code80_counter > 0;code80_counter--) {
                        fputc(raw_code->code[1],outfd);
                        fputc(raw_code->code[2],outfd);
                        raw_code_free = raw_code;
                        raw_code = raw_code->next;
                        free(raw_code_free);
                    }
                    for (;remain80 > 0;remain80 --)// fill the 3bytes block
                        fputc(0,outfd);
                }
            } else {//no more than 3 in line "80" code
                fwrite(raw_code->code,1,3,outfd);
                //	errquit("fwrite() error! \n");
                raw_code_free = raw_code;
                raw_code = raw_code->next;
                free(raw_code_free);
            }
            //code80_counter = 0;
            //	fflush(outfd);//for debug
        }
        // in case of extra byte in the end
        if (extra_byte) {
            fputc(0,outfd);
            fputc(32,outfd);
            fputc(0,outfd);
        }
        free(raw_code);
        if (freeindex() != 0)
            errquit("freeindex error!\n");
    }
    free (file_read_buf1);
    free (file_read_buf2);
    return (0);
}
static int uncompress(FILE const* infd,FILE const* outfd,int mode) {
    int decode_counter=0,read_counter=0,code_remain=0;
    int i,code_type,offset,length,length80,next_char,repeat_times,code_num=0;
    unsigned char (* raw_code_p)[3];
    static unsigned char *switch_read_buf;

    //cause CODE may change CODE_LENGTH calculate here
//	CODE_LENGTH = sizeof(CODE);
//	CODE_BUF_SIZE = CODE_LENGTH*CODE_NUM;

    file_read_buf1 = (unsigned char *)malloc(CODE_BUF_SIZE);
    file_read_buf2 = (unsigned char *)malloc(CODE_BUF_SIZE);
    decode_buf = (unsigned char *)malloc(DECODE_BUF_SIZE);
    raw_code_p = file_read_buf1;
    window_back = decode_buf - 1;//initial the window_back ptr

    while ((code_num = fread(file_read_buf2 + code_remain *3 ,CODE_LENGTH,CODE_NUM - code_remain,infd) + code_remain) > 0) {
        //printf("%d\n",*(raw_code_p+1)[0]);

        if (code_remain != 0) {
            memcpy(file_read_buf2,raw_code_p,code_remain * 3);
        }
        //for efficiency here i switch the pointer of buf instead of memcpy the
        //larger block of memory
        switch_read_buf = file_read_buf1;
        file_read_buf1 = file_read_buf2;
        file_read_buf2 = switch_read_buf;

        raw_code_p = file_read_buf1;

        for (decode_counter=1;decode_counter <= code_num;decode_counter++) {

            if ((*raw_code_p)[0]&128) {//code first bit is 1
                code_type = 1;
                i = repeat_times = ((*raw_code_p)[0] & 127)+1;
            } else {//code first bit is 0
                if ((length = (*raw_code_p)[1] & 31) || ((*raw_code_p)[0] == 0 && (*raw_code_p)[1] == 0)) {//not the continuous "80" code
                    code_type = 0;
                    offset = (((*raw_code_p)[0] & 127)<<3) + (((*raw_code_p)[1] & 224) >> 5);

                } else {//continuous "80" or "backspace" code
                    code_type = 2;
                    length80 = (((*raw_code_p)[0] & 127)<<3) + (((*raw_code_p)[1] & 224) >> 5);
                }
            }

            //debug!!
            //code_type == 0 \
            //&& offset == 0 )
            //(*(raw_code_p))[0] == 0
            //	if ((*(raw_code_p+3))[0] == 10 && (*(raw_code_p+3))[1] == 82 && (*(raw_code_p+3))[2] == 203){
            //		printf("000000000 %d \n",code_type);
            //	}
            //debug!!

            //process the rawcode
            switch (code_type) {
            case 0: //code type 0
                //length != 0 ;length == 0 means it's continuous 80
                //	printf("0");
                memcpy(window_back+1,window_back - WINSIZE + offset +1,length);
                window_back += length;

                *(++window_back) = (*raw_code_p)[2];

                raw_code_p++;
                break;

            case 1: //code type 1
                //	printf("1");
                for (;i > 0;i--) {
                    *(++window_back) =  (*raw_code_p)[1];
                    *(++window_back) =  (*raw_code_p)[2];

                }
                raw_code_p++;
                break;


            case 2://code type 2 continuous"80" code
                //	printf("2");
                if (length80 == 1) {//an extra code in the end
                    window_back--;
                    raw_code_p++;
                } else {
                    memcpy(window_back+1,(char *)(raw_code_p + 1),length80);
                    raw_code_p += length80 % 3 ?(length80 /3 + 2):(length80 /3 + 1);
                    window_back += length80;
                    decode_counter += length80 % 3 ?(length80 /3 + 1):(length80 /3);

                }
                break;
            }
            if ((decode_buf + DECODE_BUF_SIZE - window_back -1 <  MAX_80_NUM_DECODE + 1)) {
                fwrite(decode_buf,1,window_back-WINSIZE-decode_buf+1,outfd);//write and reset decode_buf
                memcpy(decode_buf,window_back-WINSIZE+1,WINSIZE);
                window_back = decode_buf+WINSIZE-1;
            }


            if ((code_remain = code_num - decode_counter) < MAX_80_CODE  && code_num == CODE_NUM)
                break;
        }
    }
    fwrite(decode_buf,1,window_back-decode_buf+1,outfd);//write the rest code
    return (0);
}
static void help() {
    printf(
        "   Examples:\n"
        "     zax -c bar.zax foo         #Create bar.zax from file foo\n"
        "     zax -x bar.zax [foo]       #Uncompress a file from bar.zax name it foo,foo is optional\n\n"
        "     zax foo                    #zax will judge the usage.if 'foo' is a zaxed file zax will"
        "                                 uncompress it else zax will compress it with name foo.zax"
        "   Usage: zax [OPTION...] [FILE...]\n"
        "     -c bar          create a new archive named bar\n"
        "     -x foo          uncompress a file named foo\n"
        "     -h              help menu (this screen)\n"
        "     -v              set verbose mode\n"

        "Compress/Uncompress program ZAX made by auxten\n"
        "       ---(ZAX is named for some reason...)\n"
        "Bug or suggestion please contact auxtenwpc[AT]gmail[DOT]com\n"
    );
}
int main(int argc, char **argv) {

    int mode=0;
    int verbose=0;
    char usage='_';//default usage ie, drag'n'drop & single-argument mode
    unsigned char *infname = NULL;
    unsigned char *outfname = NULL;
    unsigned char *fname = NULL;
    unsigned char file_name_len=0;
    unsigned char char_zax[3];

    /* check arguments */
    // printf ("argc:%d,argv:%s\n",argc,*argv);
    if (argc == 1) {
        help();
        exit(0);
    }
    while (1) {
        int c=0;
        if (-1 != (c = getopt(argc, argv, "-c:x:hv"))) {
            switch (c) {
            case 'c':
                usage='c';
                outfname = optarg;
                break;
            case 'x':
                usage='x';
                infname = optarg;
                break;
            case 'h':
                help();
                exit(0);
            case 'v':
                verbose = 1;
                break;
            case  1 :
                (usage=='x')?(outfname = optarg):(infname = optarg);
                break;
            }
            // printf ("c:%d\n",c);
        } else {
            // printf ("else c:%d\n",c);
            break;
        }
    }
    // drag'n'drop & single-argument mode
    if (usage == '_') {
        int i=0;
        inputfd = fopen(argv[1],"rb");
        if (3 != fread(char_zax,1,3,inputfd))
            errquit("fread error!\n");
        if (0 != fclose(inputfd))
            errquit("fclose error!\n");
        //judge usage 'x' or 'c'
        for (i=0;i != 3;i++) {
            if (char_zax[i] == fileheader[i]) {
                usage = 'x';
                continue;
            } else {
                usage = 'c';
                break;
            }
        }
        infname = argv[1];
        switch (usage) {
        case 'x':
            outfname = NULL;
            break;
        case 'c':
            outfname = calloc(strlen(argv[1])+5,1);
            outfname = strcat(outfname,argv[1]);
            outfname = strcat(outfname,".zax");
            break;
        }
    }
    // printf ("in: %s out: %s",infname,outfname);


    //initialize the pthread attrib for every worker
    pthread_attr_init(&attr);
    //set attr state for join() in the mother
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    instream_buf = (char *)malloc(FILE_BUF_SIZE1*640);
    outstream_buf = (char *)malloc(FILE_BUF_SIZE1*640);
    if ((inputfd = fopen(infname,"rb")) == NULL)
        errquit("inputfile open error!\n");

    file_name_len = (unsigned char)(strlen(infname));
    if (usage == 'c') {
        if ((outputfd = fopen(outfname,"w+b")) == NULL)
            errquit("outputfile open error!\n");

        if ( 3 != fwrite(fileheader,1,3,outputfd))
            errquit("fwrite error!\n");
        if ( 1 != fwrite(&file_name_len,1,1,outputfd))
            errquit("fwrite error!\n");
        if ( file_name_len != fwrite(infname,1,file_name_len,outputfd))
            errquit("fwrite error!\n");
        // printf ("c\n");
        // printf ("in: %s out: %s\n",infname,outfname);
        if (compress (inputfd,outputfd,mode))
            errquit("compress error!\n");
    } else if (usage == 'x') {
        // printf ("x\n");
        // printf ("in: %s out: %s\n",infname,outfname);
        if (3 != fread(char_zax,1,3,inputfd))
            errquit("fread error!\n");
        if (char_zax[0] == fileheader[0] \
                && char_zax[1] == fileheader[1] \
                && char_zax[2] == fileheader[2]) {

            if (1 != fread(&file_name_len,1,1,inputfd))
                errquit("fread error!\n");
            if (file_name_len > 0/* && outfname == NULL*/) {
                fname = calloc(file_name_len+1,1);
                if (file_name_len != fread(fname,1,file_name_len,inputfd))
                    errquit("fread error!\n");
                if (outfname == NULL) outfname = fname;
            }
            if ((outputfd = fopen(outfname,"w+b")) == NULL)
                errquit("outputfile open error!\n");

            if (uncompress (inputfd,outputfd,mode))
                errquit("compress error!\n");
        } else {
            errquit("not zax file!\n");
        }
    }

    pthread_attr_destroy(&attr);

#ifdef WIN_32
    if (_fcloseall() == EOF)
#else
    if (fcloseall() == EOF)
#endif //WIN_32
        errquit("fileclose error!\n");
    printf("%d\n",extra_byte);
//     system("PAUSE");
    exit (0);
}
