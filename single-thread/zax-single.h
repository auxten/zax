#ifndef ZAX_H
#define ZAX_H

#include "./freegetopt/getopt.h"

#define WINSIZE 1024
#define MAX_MATCH_CHAR 32
#define WIN_NUM 128
#define FILE_BUF_SIZE1 WIN_NUM * WINSIZE
#define CODE_LEN 3
#define MAX_CODE_CHAIN_LEN 10240000
#define FILE_BUF_SIZE2 WIN_NUM * WINSIZE
#define MAX_DECODE_LEN 256
#define MAX_80_NUM 511

#define CODE_NUM 1024
#define DECODE_BUF_SIZE 102400
#define CODE_LENGTH 3
#define CODE_BUF_SIZE  CODE_LENGTH*CODE_NUM
#define MAX_80_NUM_DECODE 1023
#define MAX_80_CODE 342

FILE *inputfd,*outputfd;
static unsigned char *file_read_buf1,*file_read_buf2;
static unsigned char *window_back;
static unsigned char fileheader[260]={'z','A','x'};
typedef struct index_chain{
		int position;
		struct index_chain * next;
} CHAIN;

typedef struct rawcode_chain{
		unsigned char code[3];
		struct rawcode_chain * next;
} CODE;

CODE * raw_code=NULL;
CHAIN hash_index[256][256]={0};

//function declaration
static void errquit(const char *);
static int uncompress(FILE const*,FILE const*,const int mode); //return 0 for success,1 for fail
static int compress(FILE const*,FILE const*,const int mode); //return 0 for success,1 for fail
static int makeindex(unsigned char *,int);
static int freeindex(void);
static CODE * genrawcode(void);
static void errquit(const char *);
static void help(void);

#endif //ZAX_H
