/* Action 1 native Snake loader. Fixed addresses, exact preflight, no flash I/O.
 * On failure unhook the GUI but retain staged unused-UVC code until power off:
 * restoring instructions under a potentially suspended RTOS PC is unsafe.
 */
#define _POSIX_C_SOURCE 200809L
#include <sys/mman.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
struct Patch { uint32_t address,old,new; };
struct Signature { uint32_t address,words[4]; };
#include "loader_payload.h"
#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
#ifndef SNAKE_ROOT
#define SNAKE_ROOT ""
#endif
#define PENDING SNAKE_ROOT "/tmp/user_data/oa_snake_failed"
static int memfd=-1,staged=0,hooked=0;
static volatile sig_atomic_t stopped;
struct Map { uintptr_t address; size_t length; void *memory; };
static struct Map maps[24];static unsigned nmap;
static void pause_ms(unsigned ms) { struct timespec t={ms/1000,(long)(ms%1000)*1000000};while(nanosleep(&t,&t)&&errno==EINTR&&!stopped){} }
static void status(const char *s) { FILE *f=fopen(SNAKE_ROOT "/tmp/oa_snake.status","w");if(f){fprintf(f,"%s\n",s);fclose(f);}fprintf(stderr,"snake: %s\n",s); }
static void *at(uintptr_t address,size_t length) {
    for(unsigned i=0;i<nmap;i++) if(address>=maps[i].address && address+length<=maps[i].address+maps[i].length)
        return (char *)maps[i].memory+address-maps[i].address;
    if(nmap==COUNT(maps)){fprintf(stderr,"map limit\n");exit(1);}
    uintptr_t page=address&~(uintptr_t)4095;size_t size=(address+length-page+4095)&~(size_t)4095;
    void *p=mmap(NULL,size,PROT_READ|PROT_WRITE,MAP_SHARED,memfd,(off_t)page);
    if(p==MAP_FAILED){perror("mmap");exit(1);}
    maps[nmap++]=(struct Map){page,size,p};return (char *)p+address-page;
}
static uint32_t get(uint32_t a) { return *(volatile uint32_t *)at(a,4); }
static void set(uint32_t a,uint32_t v) { *(volatile uint32_t *)at(a,4)=v;__sync_synchronize(); }
/* /dev/mem is Device memory on this kernel. libc memcpy/memcmp may issue
 * unaligned SIMD loads and fault; every mapped access must stay aligned. */
static void memory_read(uint32_t a,void *out,size_t n) {
    unsigned char *b=out;
    for(size_t i=0;i<n;i+=4){uint32_t v=get(a+(uint32_t)i);memcpy(b+i,&v,4);}
}
static int memory_equal(uint32_t a,const void *source,size_t n) {
    const unsigned char *b=source;
    for(size_t i=0;i<n;i+=4){uint32_t v;memcpy(&v,b+i,4);if(get(a+(uint32_t)i)!=v)return 0;}
    return 1;
}
static void memory_write(uint32_t a,const void *source,size_t n) {
    const unsigned char *b=source;
    for(size_t i=0;i<n;i+=4){uint32_t v;memcpy(&v,b+i,4);set(a+(uint32_t)i,v);}
}
static int exists(const char *p) { return access(p,F_OK)==0; }
static int disabled(void) { return exists(SNAKE_ROOT "/tmp/SD0/OA_SNAKE_DISABLE")||exists(SNAKE_ROOT "/tmp/user_data/OA_SNAKE_DISABLE"); }
static int failure_flag(void) {
    int fd=open(PENDING,O_WRONLY|O_CREAT|O_TRUNC,0600);
    if(fd<0)return 0;
    const char message[]="failed or interrupted\n";
    int ok=write(fd,message,22)==22;
    /* Ambafs on the camera has no fsync operation. Require successful native
     * file close and read-back there; never ignore actual write/close errors. */
    if(fsync(fd)&&errno!=EINVAL&&errno!=ENOSYS&&errno!=EOPNOTSUPP)ok=0;
    if(close(fd))ok=0;
    fd=open(PENDING,O_RDONLY);if(fd<0)return 0;
    char check[23]={0};ok=read(fd,check,sizeof(check))==22 && !memcmp(check,message,22) && ok;
    if(close(fd))ok=0;
    fd=open(SNAKE_ROOT "/tmp/user_data",O_RDONLY|O_DIRECTORY);
    if(fd<0)return 0;
    if(fsync(fd)&&errno!=EINVAL&&errno!=ENOSYS&&errno!=EOPNOTSUPP)ok=0;
    if(close(fd))ok=0;
    return ok;
}
static void signal_stop(int sig) { (void)sig;stopped=1; }
static int original_patches(const struct Patch *p,size_t n) {
    for(size_t i=0;i<n;i++)if(get(p[i].address)!=p[i].old)return 0;
    return 1;
}
static void unhook(void) {
    if(!staged)return;
    set(DATA_ADDRESS+8,0);pause_ms(1500);
    int captures=*(volatile uint16_t *)at(DATA_ADDRESS+CAPTURE_OFFSET,2) ||
                 *(volatile uint16_t *)at(DATA_ADDRESS+ENTRY_OFFSET,2);
    if(hooked)for(size_t i=0;i<COUNT(patches);i++) {
        /* Keep capture-draining input wrappers if a finger is still down.
         * allow=0 prevents new game captures; they forward subsequent touches.
         * Staged code/data remains valid until the next full power cycle. */
        if(captures&&(patches[i].address==0x8eca20||patches[i].address==0x8eca24))continue;
        if(get(patches[i].address)==patches[i].new)set(patches[i].address,patches[i].old);
    }
    /* Retain both UVC init guards until power off: its code is still staged. */
    status("disabled_until_power_cycle");
}
int main(int argc,char **argv) {
    int check=argc==2&&!strcmp(argv[1],"--check");
    unsigned trial=argc==3&&!strcmp(argv[1],"--trial")?(unsigned)strtoul(argv[2],NULL,10):0;
    if(argc!=1&&!check&&!trial){fprintf(stderr,"Usage: oa_snake [--check | --trial seconds]\n");return 2;}
    int lock=open(SNAKE_ROOT "/tmp/oa_snake.lock",O_RDWR|O_CREAT,0600);if(lock<0||flock(lock,LOCK_EX|LOCK_NB))return 0;
    if(disabled()){status("disabled");return 0;}
    if(!check&&!trial&&exists(PENDING)){status("previous_failure");return 1;}
    memfd=open(SNAKE_ROOT "/dev/mem",O_RDWR|O_SYNC);if(memfd<0){perror("mem");return 1;}
    if(!original_patches(patches,COUNT(patches))||!original_patches(uvc_gates,COUNT(uvc_gates))||!memory_equal(CODE_ADDRESS,code_old,sizeof(code_old))){status("unsupported_or_already_loaded");return 1;}
    for(size_t i=0;i<COUNT(signatures);i++)if(!memory_equal(signatures[i].address,signatures[i].words,16)){status("signature_mismatch");return 1;}
    /* InitZero would zero the sample's first two words. Require uninitialized
     * sample state, and reject either ThreadX thread ID. This is a static
     * reservation, never a claim that an arbitrary heap address is free. */
    if(get(0x25ca628)<=1||get(0x25ca62c)<=1||get(DATA_ADDRESS)==0x54485244||get(0x25cadb0)==0x54485244){status("uvc_sample_in_use");return 1;}
    unsigned char snapshot[0x510];memory_read(DATA_ADDRESS,snapshot,sizeof(snapshot));pause_ms(1000);
    if(!memory_equal(DATA_ADDRESS,snapshot,sizeof(snapshot))){status("uvc_data_changing");return 1;}
    if(check){status("preflight_passed");return 0;}
    /* Complete all mappings before mutation, so a mapping failure cannot leave
     * installed hooks without cleanup. */
    at(CODE_ADDRESS,sizeof(code_new));at(LABEL_ADDRESS,sizeof(labels));
    if(!failure_flag()){status("cannot_arm_failure_guard");return 1;}
    signal(SIGTERM,signal_stop);signal(SIGINT,signal_stop);
    for(size_t i=0;i<COUNT(uvc_gates);i++)set(uvc_gates[i].address,uvc_gates[i].new);
    staged=1;
    memory_write(CODE_ADDRESS,code_new,sizeof(code_new));
    for(unsigned i=0;i<CONTEXT_BYTES;i+=4)set(DATA_ADDRESS+i,0);
    for(unsigned i=0x4c0;i<0x510;i+=4)set(DATA_ADDRESS+i,0);
    memory_write(LABEL_ADDRESS,labels,sizeof(labels));
    if(!memory_equal(CODE_ADDRESS,code_new,sizeof(code_new))||!memory_equal(LABEL_ADDRESS,labels,sizeof(labels))){status("stage_verify_failed");unhook();return 1;}
    set(DATA_ADDRESS,MAGIC);set(DATA_ADDRESS+4,1);
    /* Game lifetime belongs to the RTOS GUI. Linux scheduling delays must
     * never expire a healthy game; explicit stop and native busy still exit. */
    set(DATA_ADDRESS+12,UINT32_MAX);set(DATA_ADDRESS+8,1);
    for(size_t i=0;i<COUNT(patches);i++)set(patches[i].address,patches[i].new);
    hooked=1;status("starting");
    uint32_t last=0;unsigned stale=0,ticks=0;int healthy=0;
    while(!stopped) {
        pause_ms(500);ticks++;
        uint32_t beat=get(DATA_ADDRESS+16);
        if(beat==last)stale++;else stale=0;
        last=beat;
        if(stale>120){failure_flag();status("heartbeat_failed");break;}
        if(disabled()){unlink(PENDING);break;}
        if(!healthy&&ticks>=6&&beat){healthy=1;status("ready");}
        if(ticks==120&&healthy)unlink(PENDING);
        if(trial&&ticks>=trial*2){if(healthy)unlink(PENDING);else failure_flag();break;}
    }
    unhook();
    for(unsigned i=0;i<nmap;i++)munmap(maps[i].memory,maps[i].length);
    close(memfd);close(lock);return healthy?0:1;
}
