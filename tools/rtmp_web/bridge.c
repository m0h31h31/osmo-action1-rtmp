#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Only cmd_rlv_transfer SHA256 81839ce6740348b230c926942aaade31991deb0acaa8988403eaddab2e89db1e.
 * Start before S99bootdone. Native callback receives this boot's shared buffer.
 * No saved handshake, /dev/mem access, init replay or automatic start command. */
#define ROOT "/tmp/oa_rtmp_web/"
static unsigned crc(const unsigned char *b,int n,unsigned seed,unsigned poly){
    for(int i=0;i<n;i++){seed^=b[i];for(int j=0;j<8;j++)seed=seed&1?(seed>>1)^poly:seed>>1;}return seed;
}
static int valid(const unsigned char *b,int n){
    if(n<17||n>256||b[0]!=0x55||(b[1]|((b[2]&3)<<8))!=n||b[2]>>2!=1)return 0;
    if(crc(b,3,0x77,0x8c)!=b[3]||crc(b,n-2,0x3692,0x8408)!=(unsigned)(b[n-2]|b[n-1]<<8))return 0;
    if(b[4]!=2||b[5]!=1||b[8]!=0x40||b[9]!=2||b[10]!=0x8e)return 0;
    if(n==17&&b[11]==0&&b[12]==1&&b[13]==28&&b[14]==0)return 1;
    if(n==19&&b[11]==1&&b[12]==1&&b[13]==26&&b[14]==0&&b[15]==1&&b[16]<=1)return 1;
    if(n>=30&&b[11]==1&&b[12]==1&&b[13]==25&&b[14]==0&&b[15]==n-18&&b[16]==0){
        unsigned rate=b[17]|b[18]<<8, len=b[25]|b[26]<<8;
        if(rate<500||rate>6000||len<9||len>200||n!=(int)(29+len)||b[n-3]!=0)return 0;
        for(int i=19;i<25;i++)if(b[i])return 0;
        if(memcmp(b+27,"rtmp://",7))return 0;
        for(unsigned i=0;i<len-1;i++)if(b[27+i]<=32||b[27+i]>=127)return 0;
        return 1;
    }
    return 0;
}
#ifndef TESTING
static void status(const char *text){FILE *f=fopen(ROOT "bridge.status","w");if(f){fputs(text,f);fclose(f);}}
static void *worker(void *unused){
    (void)unused;unlink(ROOT "bridge.ready");status("waiting_native_init\n");
    volatile unsigned *initialized=(volatile unsigned*)0x4769fc;
    volatile uintptr_t *shared=(volatile uintptr_t*)0x476a00;
    /* Give original main() time to initialize queue/IMC and process S99. */
    sleep(5);
    while(*initialized!=1||!*shared||*shared==(uintptr_t)-1)usleep(100000);
    FILE *f=fopen(ROOT "bridge.ready.new","w");
    if(!f){status("ready_file_failed\n");return NULL;}
    fprintf(f,"%d\n",(int)getpid());fclose(f);rename(ROOT "bridge.ready.new",ROOT "bridge.ready");
    status("ready\n");
    for(;;){
        f=fopen(ROOT "duml.req","rb");
        if(f){
            unsigned char b[512];int n=fread(b,1,sizeof(b),f);fclose(f);unlink(ROOT "duml.req");
            if(valid(b,n))((int(*)(void*,int))0x4050d8)(b,n);
        }
        usleep(100000);
    }
    return NULL;
}
__attribute__((constructor)) static void init(void){
    unsetenv("LD_PRELOAD");pthread_t thread;
    if(!pthread_create(&thread,NULL,worker,NULL))pthread_detach(thread);
}
#else
int main(void){unsigned char b[512];int n=fread(b,1,sizeof(b),stdin);puts(valid(b,n)?"valid":"invalid");return 0;}
#endif
