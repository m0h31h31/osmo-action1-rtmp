#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>

static char cfgpath[512], tokenpath[512], lockpath[512];
#ifdef PERSISTENT_BRIDGE
#define REQUEST_PATH "/tmp/oa_rtmp_web/duml.req"
#else
#define REQUEST_PATH "/tmp/oa_repair/duml.req"
#endif
static int lockfd=-1, debugging=0;
static const char *env(const char *s) { const char *v=getenv(s); return v?v:""; }
static void ipc(const char *s) {
    int f=open("/tmp/ipc_fifo",O_WRONLY|O_CREAT|O_TRUNC|O_NONBLOCK,0600);
    if(f>=0){write(f,s,strlen(s));close(f);}
}
static void cleanup(void) {
    if(debugging){ipc("9wifi_swudp\n");usleep(1100000);ipc("9wifi_gnd\n");usleep(1100000);debugging=0;}
    if(lockfd>=0){close(lockfd);lockfd=-1;}
}
static void interrupted(int sig) { cleanup();_exit(128+sig); }
static void js(const char *s) {
    putchar('"'); for(const unsigned char *p=(const unsigned char*)s;*p;p++){
        if(*p=='"'||*p=='\\')printf("\\%c",*p);
        else if(*p<32)printf("\\u%04x",*p);else putchar(*p);
    }putchar('"');
}
static _Noreturn void fail(const char *code,const char *message){
    printf("{\"ok\":false,\"code\":");js(code);printf(",\"message\":");js(message);puts("}");exit(0);
}
static void paths(void){
    const char *persistent="/tmp/user_data/oa_rtmp_web",*runtime="/tmp/oa_rtmp_web";
#ifdef TESTING
    persistent=runtime=env("OA_TEST_ROOT");
#endif
    snprintf(cfgpath,sizeof(cfgpath),"%s/config",persistent);
    snprintf(tokenpath,sizeof(tokenpath),"%s/token",runtime);
    snprintf(lockpath,sizeof(lockpath),"%s/lock",runtime);
}
static int filetext(const char *path,char *buf,size_t size){
    FILE *f=fopen(path,"r");if(!f)return 0;
    size_t n=fread(buf,1,size-1,f);buf[n]=0;fclose(f);return 1;
}
static int number(const char *s,long min,long max){
    if(!*s)return -1;for(const char *p=s;*p;p++)if(!isdigit((unsigned char)*p))return -1;
    char *end;errno=0;long n=strtol(s,&end,10);return errno||*end||n<min||n>max?-1:(int)n;
}
static int validurl(const char *s){
    size_t n=strlen(s);if(n<9||n>=200||strncmp(s,"rtmp://",7))return 0;
    const char *slash=strchr(s+7,'/');if(!slash||slash==s+7||!slash[1])return 0;
    for(size_t i=0;i<n;i++)if((unsigned char)s[i]<=32||(unsigned char)s[i]>=127)return 0;
    return 1;
}
static void config(char *url,int *rate){
    strcpy(url,"rtmp://192.168.100.6:1935/live/osmo");*rate=2000;
    char b[512];if(!filetext(cfgpath,b,sizeof(b)))return;
    char *nl=strchr(b,'\n');if(!nl)return;*nl++=0;nl[strcspn(nl,"\r\n")]=0;
    int r=number(nl,500,6000);if(validurl(b)&&r>0){strcpy(url,b);*rate=r;}
}
static int atomicwrite(const char *path,const void *buf,size_t n){
    char temp[550];snprintf(temp,sizeof(temp),"%s.new.%d",path,(int)getpid());
    int f=open(temp,O_WRONLY|O_CREAT|O_EXCL,0600);if(f<0)return 0;
    int ok=write(f,buf,n)==(ssize_t)n;
    if(fsync(f)&&errno!=EINVAL)ok=0;
    if(close(f))ok=0;
    if(ok&&rename(temp,path)==0)return 1;unlink(temp);return 0;
}
static int hex(int c){if(c>='0'&&c<='9')return c-'0';c=tolower(c);return c>='a'&&c<='f'?c-'a'+10:-1;}
static int decode(char *s){
    char *d=s;for(char *p=s;*p;p++){
        int c=(unsigned char)*p;
        if(c=='%'){if(!p[1]||!p[2]||hex(p[1])<0||hex(p[2])<0)return 0;c=hex(p[1])*16+hex(p[2]);p+=2;}
        else if(c=='+')c=' ';
        if(c==0||c<32||c==127)return 0;*d++=(char)c;
    }*d=0;return 1;
}
static unsigned crc(const unsigned char *b,int n,unsigned seed,unsigned poly){
    for(int i=0;i<n;i++){seed^=b[i];for(int j=0;j<8;j++)seed=seed&1?(seed>>1)^poly:seed>>1;}return seed;
}
static int packet(unsigned char *out,const unsigned char *payload,int n,unsigned seq){
    unsigned length=n+13;out[0]=0x55;out[1]=length&255;out[2]=(length>>8)|4;
    out[3]=crc(out,3,0x77,0x8c);out[4]=2;out[5]=1;out[6]=seq&255;out[7]=seq>>8;
    out[8]=0x40;out[9]=2;out[10]=0x8e;memcpy(out+11,payload,n);
    unsigned c=crc(out,n+11,0x3692,0x8408);out[n+11]=c&255;out[n+12]=c>>8;return length;
}
static int reply(const char *path,unsigned seq,unsigned char *out,const struct stat *before){
    FILE *f=fopen(path,"r");if(!f)return 0;
    struct stat now;
    if(fstat(fileno(f),&now)){fclose(f);return 0;}
    off_t offset=(now.st_ino==before->st_ino&&now.st_dev==before->st_dev&&now.st_size>=before->st_size)?before->st_size:0;
    if(now.st_size-offset>65536)offset=now.st_size-65536;
    if(fseeko(f,offset,SEEK_SET)){fclose(f);return 0;}
    char line[1800];int result=0;
    while(fgets(line,sizeof(line),f)){
        if(line[0]!='[')continue;char *p=strstr(line,"]:55 ");if(!p)continue;p+=2;
        unsigned char b[512];int n=0;
        while(*p&&n<512){while(*p==' ')p++;if(!p[0]||!p[1]||hex(p[0])<0||hex(p[1])<0)break;b[n++]=hex(p[0])*16+hex(p[1]);p+=2;}
        if(n<14||b[4]!=1||b[5]!=2||b[6]!=(seq&255)||b[7]!=(seq>>8)||b[8]!=0x80||b[9]!=2||b[10]!=0x8e)continue;
        if((b[1]|((b[2]&3)<<8))!=n||crc(b,3,0x77,0x8c)!=b[3]||crc(b,n-2,0x3692,0x8408)!=(unsigned)(b[n-2]|b[n-1]<<8))continue;
        if(b[11]){result=-1;break;}result=n-14;memcpy(out,b+12,result);break;
    }fclose(f);return result;
}
static int request(const unsigned char *payload,int n,unsigned char *out){
    unsigned short seq=0;int r=open("/dev/urandom",O_RDONLY);if(r<0||read(r,&seq,2)!=2){if(r>=0)close(r);fail("random","无法产生请求序号");}close(r);
    unsigned char b[512];int length=packet(b,payload,n,seq);
    struct stat before={0};stat("/tmp/dbg_out_wifi_swudp",&before);
    if(!atomicwrite(REQUEST_PATH,b,length))fail("write_failed","无法提交相机命令");
    for(int i=0;i<80;i++){usleep(100000);int k=reply("/tmp/dbg_out_wifi_swudp",seq,out,&before);if(k<0)fail("rejected","相机拒绝了命令");if(k>0)return k;}
    fail("timeout","相机未确认命令，请刷新状态核实；不要假定操作成功");return 0;
}
static int state(unsigned char *out){
    const unsigned char query[]={0,1,28,0};unsigned char r[512];int n=request(query,4,r);
    if(n!=14||memcmp(r,query,4)||r[4]!=9)fail("bad_reply","相机返回了无法识别的状态");
    memcpy(out,r+5,9);return (out[1]&7)==0;
}
static void bridge(void){
#ifdef PERSISTENT_BRIDGE
    char ready[64];
    if(!filetext("/tmp/oa_rtmp_web/bridge.ready",ready,sizeof(ready)))fail("bridge_missing","控制服务尚未初始化完成，请稍后刷新；若持续异常请查看启动日志");
    ready[strcspn(ready,"\r\n")]=0;
    int pid=number(ready,2,4194304);
    if(pid<0||kill(pid,0))fail("bridge_missing","控制服务未运行，请检查启动日志");
    char path[96],command[128];snprintf(path,sizeof(path),"/proc/%d/comm",pid);
    if(!filetext(path,command,sizeof(command))||strcmp(command,"cmd_rlv_transfe\n")){
        /* Linux TASK_COMM_LEN is 16; the 16-char executable name is truncated. */
        snprintf(path,sizeof(path),"/proc/%d/cmdline",pid);
        if(!filetext(path,command,sizeof(command))||strcmp(command,"/usr/bin/cmd_rlv_transfer"))fail("bridge_missing","控制服务进程不匹配");
    }
#else
    if(access("/tmp/oa_repair/duml_bridge5.so",R_OK))fail("bridge_missing","临时控制桥未就绪，相机可能已重启，需要先恢复控制服务");
#endif
#ifdef PERSISTENT_BRIDGE
    /* Bound RAM use across repeated web polling. This operation holds the lock. */
    int logfd=open("/tmp/dbg_out_wifi_swudp",O_WRONLY|O_CREAT|O_TRUNC,0600);
    if(logfd<0)fail("debug_log","无法准备状态回复日志");
    close(logfd);
#endif
    debugging=1;ipc("8wifi_swudp\n");usleep(1100000);
}
int main(int argc,char **argv){
#ifdef TESTING
    if(argc==2&&!strcmp(argv[1],"--packet")){unsigned char b[64],p[]={0,1,28,0};int n=packet(b,p,4,0x4001);for(int i=0;i<n;i++)printf("%02x",b[i]);puts("");return 0;}
    if(argc==4&&!strcmp(argv[1],"--reply-fixture")){struct stat before={0};stat(argv[2],&before);before.st_size=atol(argv[3]);unsigned char b[512];int n=reply(argv[2],0x4001,b,&before);printf("%d:",n);for(int i=0;i<n;i++)printf("%02x",b[i]);puts("");return 0;}
#else
    (void)argc;(void)argv;
#endif
    puts("Content-Type: application/json; charset=utf-8\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\n");
    paths();atexit(cleanup);signal(SIGTERM,interrupted);signal(SIGINT,interrupted);signal(SIGALRM,interrupted);alarm(35);
    char token[80],url[256];int rate;config(url,&rate);
    if(!filetext(tokenpath,token,sizeof(token))||strlen(token)!=64)fail("not_ready","网页控制接口尚未部署完整");
    if(!strcmp(env("REQUEST_METHOD"),"GET")){
        printf("{\"ok\":true,\"url\":");js(url);printf(",\"bitrate\":%d,\"token\":",rate);js(token);puts("}");return 0;
    }
    if(strcmp(env("REQUEST_METHOD"),"POST"))fail("method","只支持 GET 和 POST");
    if(strcmp(env("CONTENT_TYPE"),"application/x-www-form-urlencoded"))fail("content_type","请求格式错误");
    char origin[300];snprintf(origin,sizeof(origin),"http://%s",env("HTTP_HOST"));
    if(*env("HTTP_ORIGIN")&&strcmp(origin,env("HTTP_ORIGIN")))fail("forbidden","请求来源不匹配");
    /* This BusyBox httpd forwards Referer but not every Origin header. */
    const char *referer=env("HTTP_REFERER");size_t olen=strlen(origin);
    if(strncmp(referer,origin,olen)||referer[olen]!='/')fail("forbidden","请从相机管理网页发起操作");
    int length=number(env("CONTENT_LENGTH"),1,1500);if(length<0)fail("length","请求长度错误");
    char body[1501];if(fread(body,1,length,stdin)!=(size_t)length)fail("length","请求内容不完整");body[length]=0;
    if(memchr(body,0,length))fail("invalid","请求包含空字节");
    char *action=NULL,*submitted=NULL,*newurl=NULL,*newrate=NULL,*saveptr;
    for(char *part=strtok_r(body,"&",&saveptr);part;part=strtok_r(NULL,"&",&saveptr)){
        char *eq=strchr(part,'=');if(!eq)fail("invalid","请求字段错误");*eq++=0;
        if(!decode(part)||!decode(eq))fail("invalid","请求编码错误");
        char **slot=!strcmp(part,"action")?&action:!strcmp(part,"token")?&submitted:!strcmp(part,"url")?&newurl:!strcmp(part,"bitrate")?&newrate:NULL;
        if(!slot||*slot)fail("invalid","重复或未知字段");*slot=eq;
    }
    if(!submitted||strcmp(token,submitted))fail("forbidden","页面令牌已失效，请重新加载网页");
    if(!action)fail("invalid","缺少操作类型");
    if(strcmp(action,"save")&&strcmp(action,"status")&&strcmp(action,"start")&&strcmp(action,"stop"))fail("invalid","未知操作");
    lockfd=open(lockpath,O_CREAT|O_RDWR,0600);
    if(lockfd<0)fail("lock","无法打开控制锁");
    if(flock(lockfd,LOCK_EX|LOCK_NB))fail("busy","另一项操作正在执行，请稍后重试");
    if(!strcmp(action,"save")){
        int v=newrate?number(newrate,500,6000):-1;
        if(!newurl||!validurl(newurl)||v<0)fail("invalid","请输入有效 RTMP 地址（少于 200 字节）及 500–6000 kbps 码率");
        char data[512];int n=snprintf(data,sizeof(data),"%s\n%d\n",newurl,v);
        if(!atomicwrite(cfgpath,data,n))fail("save_failed","配置写入失败，未确认保存");
        char check[256];int checkrate;config(check,&checkrate);
        if(strcmp(check,newurl)||checkrate!=v)fail("save_failed","配置读取校验失败");
        puts("{\"ok\":true,\"message\":\"配置已保存；当前推流不受影响，下次启动生效\"}");return 0;
    }
    bridge();unsigned char s[9],r[512];
    if(!strcmp(action,"start")){
        if(state(s))fail("already_running","相机已在推流；要应用新配置，请先停止再启动");
        unsigned char p[256]={1,1,25,0};int u=strlen(url)+1;p[4]=11+u;p[5]=0;p[6]=rate&255;p[7]=rate>>8;p[14]=u&255;p[15]=u>>8;memcpy(p+16,url,u);
        request(p,16+u,r);unsigned char start[]={1,1,26,0,1,1};request(start,6,r);
    }else if(!strcmp(action,"stop")){unsigned char stop[]={1,1,26,0,1,0};request(stop,6,r);}
    int streaming=state(s);unsigned long elapsed=s[2]|s[3]<<8|s[4]<<16|(uint32_t)s[5]<<24;int bitrate=s[6]|s[7]<<8;
    printf("{\"ok\":true,\"streaming\":%s,\"elapsed\":%lu,\"bitrate\":%d,\"message\":",streaming?"true":"false",streaming?elapsed:0,streaming?bitrate:0);
    js(!strcmp(action,"status")?"已读取相机状态":"相机已确认命令；实际收流请以 OBS 或接收服务器为准");puts("}");return 0;
}
