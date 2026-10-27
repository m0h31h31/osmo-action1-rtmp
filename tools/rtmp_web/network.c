#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#ifndef NET_ROOT
#define NET_ROOT ""
#endif
#define CARD NET_ROOT "/tmp/SD0"
#define FLAG CARD "/OA_STA_DISABLE"
static const char *env(const char *k){const char *v=getenv(k);return v?v:"";}
static int readtext(const char *p,char *b,size_t n){FILE *f=fopen(p,"r");if(!f)return 0;size_t z=fread(b,1,n-1,f);b[z]=0;fclose(f);return 1;}
static int mounted(void){char b[512];FILE*f=fopen(NET_ROOT "/proc/mounts","r");if(!f)return 0;int ok=0;while(fgets(b,sizeof b,f))if(strstr(b," /tmp/SD0 "))ok=1;fclose(f);return ok;}
static void error(const char *code){printf("{\"ok\":false,\"code\":\"%s\"}\n",code);}
static int atomic(const char *p,const char *b){char tmp[512];snprintf(tmp,sizeof tmp,"%s.new.%d",p,getpid());int fd=open(tmp,O_WRONLY|O_CREAT|O_EXCL,0600);if(fd<0)return 0;int ok=write(fd,b,strlen(b))==(ssize_t)strlen(b);if(fsync(fd)&&errno!=EINVAL&&errno!=ENOSYS&&errno!=EOPNOTSUPP)ok=0;if(close(fd))ok=0;if(ok&&rename(tmp,p)==0)return 1;unlink(tmp);return 0;}
/* Card settings are data, never a shell program. Quotes delimit literal values;
 * no interpolation, command substitution, or escape evaluation is performed. */
static int prepare(const char *source,const char *dest){
 FILE *f=fopen(source,"r");if(!f)return 2;
 char line[512],ssid[128]="",pass[128]="";int seen_s=0,seen_p=0,bad=0;
 while(fgets(line,sizeof line,f)){
  size_t n=strlen(line);if(n==sizeof(line)-1&&line[n-1]!='\n'){bad=1;break;}
  char *p=line;if((unsigned char)p[0]==0xef&&(unsigned char)p[1]==0xbb&&(unsigned char)p[2]==0xbf)p+=3;
  while(isspace((unsigned char)*p))p++;if(!*p||*p=='#')continue;
  char *eq=strchr(p,'=');if(!eq){bad=1;break;}*eq++=0;
  char *end=p+strlen(p);while(end>p&&isspace((unsigned char)end[-1]))*--end=0;
  if(strcmp(p,"ESSID")&&strcmp(p,"PASSWORD"))continue;
  while(*eq==' '||*eq=='\t')eq++;end=eq+strlen(eq);while(end>eq&&isspace((unsigned char)end[-1]))*--end=0;
  if(*eq=='\''||*eq=='"'){char q=*eq++;if(end<=eq||end[-1]!=q){bad=1;break;}*--end=0;}
  size_t len=strlen(eq);if(len>=128){bad=1;break;}
  for(size_t i=0;i<len;i++)if((unsigned char)eq[i]<32||(unsigned char)eq[i]==127)bad=1;
  if(!strcmp(p,"ESSID")){if(seen_s++)bad=1;strcpy(ssid,eq);}else{if(seen_p++)bad=1;strcpy(pass,eq);}
 }
 if(ferror(f))bad=1;fclose(f);size_t sn=strlen(ssid),pn=strlen(pass);
 if(bad||!seen_s||!seen_p||sn<1||sn>32||(pn&&pn<8)||pn>64)return 3;
 if(pn==64)for(size_t i=0;i<pn;i++)if(!isxdigit((unsigned char)pass[i]))return 3;
 char output[1024];size_t k=(size_t)sprintf(output,"ctrl_interface=/var/run/wpa_supplicant\np2p_disabled=1\nnetwork={\nssid=");
 for(size_t i=0;i<sn;i++)k+=(size_t)sprintf(output+k,"%02x",(unsigned char)ssid[i]);
 k+=(size_t)sprintf(output+k,"\nscan_ssid=1\n");
 if(!pn)k+=(size_t)sprintf(output+k,"key_mgmt=NONE\n");
 else if(pn==64)k+=(size_t)sprintf(output+k,"key_mgmt=WPA-PSK\npsk=%s\n",pass);
 else {k+=(size_t)sprintf(output+k,"key_mgmt=WPA-PSK\npsk=\"");for(size_t i=0;i<pn;i++){if(pass[i]=='"'||pass[i]=='\\')output[k++]='\\';output[k++]=pass[i];}k+=(size_t)sprintf(output+k,"\"\n");}
 strcpy(output+k,"}\n");return atomic(dest,output)?0:4;
}
int main(int argc,char **argv){
 if(argc==4&&!strcmp(argv[1],"--prepare-config"))return prepare(argv[2],argv[3]);
 puts("Content-Type: application/json; charset=utf-8\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\n");
 char token[80];if(!readtext(NET_ROOT "/tmp/oa_rtmp_web/token",token,sizeof token)||strlen(token)!=64){error("not_ready");return 0;}
 if(strcmp(env("REQUEST_METHOD"),"GET")){
  if(strcmp(env("REQUEST_METHOD"),"POST")){error("method");return 0;}
  char origin[300];snprintf(origin,sizeof origin,"http://%s",env("HTTP_HOST"));size_t n=strlen(origin);
  if(strcmp(env("CONTENT_TYPE"),"application/x-www-form-urlencoded")||(*env("HTTP_ORIGIN")&&strcmp(env("HTTP_ORIGIN"),origin))||strncmp(env("HTTP_REFERER"),origin,n)||env("HTTP_REFERER")[n]!='/'){error("forbidden");return 0;}
  const char *len=env("CONTENT_LENGTH");char *end;long l=strtol(len,&end,10);
  if(!*len||*end||l<1||l>128){error("length");return 0;}
  char body[129],on[128],off[128];if(fread(body,1,l,stdin)!=(size_t)l){error("length");return 0;}body[l]=0;
  snprintf(on,sizeof on,"action=enable&token=%s",token);snprintf(off,sizeof off,"action=disable&token=%s",token);
  if(memchr(body,0,l)||(strcmp(body,on)&&strcmp(body,off))){error("forbidden");return 0;}
  if(!mounted()){error("card_missing");return 0;}
  int fd=open(NET_ROOT "/tmp/oa_network.lock",O_CREAT|O_RDWR,0600);if(fd<0||flock(fd,LOCK_EX|LOCK_NB)){error("busy");return 0;}
  int enable=!strcmp(body,on),ok;
  if(enable){if(access(CARD "/NO_WIFI",F_OK)==0){error("legacy_disabled");close(fd);return 0;}int r=prepare(CARD "/wifi.conf",NET_ROOT "/tmp/oa_network.validate");unlink(NET_ROOT "/tmp/oa_network.validate");if(r){error("config_invalid");close(fd);return 0;}ok=unlink(FLAG)==0||errno==ENOENT;}
  else ok=atomic(FLAG,"Disabled by camera web UI. Delete this file to enable STA.\n");
  sync();if(!ok||(access(FLAG,F_OK)==0)==enable){error("save_failed");close(fd);return 0;}close(fd);
 }
 char mode[32]="",ip[INET_ADDRSTRLEN]="";readtext(NET_ROOT "/tmp/oa_network.mode",mode,sizeof mode);
 int card=mounted(),enabled=card&&access(FLAG,F_OK)!=0&&access(CARD "/NO_WIFI",F_OK)!=0,active=!strncmp(mode,"sta",3);
 int sock=socket(AF_INET,SOCK_DGRAM,0);struct ifreq req={0};strcpy(req.ifr_name,"wlan0");if(sock>=0){if(!ioctl(sock,SIOCGIFADDR,&req))inet_ntop(AF_INET,&((struct sockaddr_in*)&req.ifr_addr)->sin_addr,ip,sizeof ip);close(sock);}
 printf("{\"ok\":true,\"enabled\":%s,\"active\":\"%s\",\"pending\":%s,\"card\":%s,\"ip\":\"%s\"}\n",enabled?"true":"false",active?"sta":"system",active!=enabled?"true":"false",card?"true":"false",ip);
 return 0;
}
