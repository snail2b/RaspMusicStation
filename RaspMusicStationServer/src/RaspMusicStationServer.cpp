#include "common.h"
#include <semaphore.h>

int CommandListenerProcess(FILE *fin, FILE* fout);

#include "FeiSocket.h"
#define PORT 30001

struct FeedBackSocket
{
  bool m_resetSocket;
  FeiSocketSession m_se;
  sem_t m_start_sem;
  sem_t m_finish_sem;
};

struct CommandFeedBackThreadInfo
{
  FILE* m_fin;
  FeedBackSocket m_CDListSocket;
  FeedBackSocket m_CDStatusSocket;
  FeedBackSocket m_ListListsSocket;
  FeedBackSocket m_ListSongsSocket;
  FeedBackSocket m_ListStatusSocket;
};

void* HostProcess_CommandFeedBackThread(void* info)
{
  CommandFeedBackThreadInfo *tInfo=(CommandFeedBackThreadInfo*)info; 
  FILE* fin=(FILE*)tInfo->m_fin;
  char buffer[4096];
  char command[256];
  bool readingCDFeedBack=false;
  bool readingListLists=false;
  bool readingListSongs=false;
  bool readingListFeedBack=false;
  while (fgets(buffer,4096, fin))
  {
    char *pos;
    if ((pos=strchr(buffer, '\n')) != NULL)
      *pos = '\0';
    if (-1==sscanf(buffer,"%s",command)) continue;
    string s_command=command;
    if (s_command=="URLPlayBack")
      printf("%s\n",buffer+12);
    else if (s_command=="CDTrackInfo")
    {
      char* pinfo=buffer+12;
      sem_wait(&tInfo->m_CDListSocket.m_start_sem);
      int len=strlen(pinfo);
      pinfo[len]='\n';
      pinfo[len+1]='\0';
      tInfo->m_CDListSocket.m_se.Send(pinfo,len+1);
      sem_post(&tInfo->m_CDListSocket.m_finish_sem);
    }
    else if (s_command=="CDPlayBack")
    {
      if (!readingCDFeedBack)
      {
        sem_wait(&tInfo->m_CDStatusSocket.m_start_sem);
        readingCDFeedBack=true;
      }
      if (tInfo->m_CDStatusSocket.m_resetSocket)
      {
        readingCDFeedBack=false;
        FeiSocketSession invalid;
        tInfo->m_CDStatusSocket.m_se=invalid;
        sem_post(&tInfo->m_CDStatusSocket.m_finish_sem);        
        tInfo->m_CDStatusSocket.m_resetSocket=false;
        sem_wait(&tInfo->m_CDStatusSocket.m_start_sem);
        readingCDFeedBack=true;        
      }
      char* pinfo=buffer+11;
      int len=strlen(pinfo);
      pinfo[len]='\n';

      pinfo[len+1]='\0';      
      tInfo->m_CDStatusSocket.m_se.Send(pinfo,len+1);
      char command2[256];
      if (-1==sscanf(pinfo,"%s",command2)) continue;
      string s_command2=command2;
      if (s_command2=="Over")
      {
        readingCDFeedBack=false;
        FeiSocketSession invalid;
        tInfo->m_CDStatusSocket.m_se=invalid;
        sem_post(&tInfo->m_CDStatusSocket.m_finish_sem);        
      }      
    }
	else if (s_command=="ListLists")
    {
      char* pinfo=buffer+10;
	  if (!readingListLists)
	  {
	      sem_wait(&tInfo->m_ListListsSocket.m_start_sem);
		  readingListLists=true;
	  }
      int len=strlen(pinfo);
      pinfo[len]='\n';
      pinfo[len+1]='\0';
      tInfo->m_ListListsSocket.m_se.Send(pinfo,len+1);
	  char command2[256];
      if (-1==sscanf(pinfo,"%s",command2)) continue;
	  string s_command2=command2;
	  if (s_command2=="#End")
      {
		  readingListLists=false;
		  sem_post(&tInfo->m_ListListsSocket.m_finish_sem);
	  }
    }
	else if (s_command=="ListSongs")
    {
      char* pinfo=buffer+10;
	  if (!readingListSongs)
	  {
	      sem_wait(&tInfo->m_ListSongsSocket.m_start_sem);
		  readingListSongs=true;
	  }
      int len=strlen(pinfo);
      pinfo[len]='\n';
      pinfo[len+1]='\0';
      tInfo->m_ListSongsSocket.m_se.Send(pinfo,len+1);
	  char command2[256];
      if (-1==sscanf(pinfo,"%s",command2)) continue;
	  string s_command2=command2;
	  if (s_command2=="#End")
      {
		  readingListSongs=false;
		  sem_post(&tInfo->m_ListSongsSocket.m_finish_sem);
	  }
    }
	else if (s_command=="ListPlayBack")
    {
      if (!readingListFeedBack)
      {
        sem_wait(&tInfo->m_ListStatusSocket.m_start_sem);
        readingListFeedBack=true;
      }
      if (tInfo->m_ListStatusSocket.m_resetSocket)
      {
        readingListFeedBack=false;
        FeiSocketSession invalid;
        tInfo->m_ListStatusSocket.m_se=invalid;
        sem_post(&tInfo->m_ListStatusSocket.m_finish_sem);        
        tInfo->m_ListStatusSocket.m_resetSocket=false;
        sem_wait(&tInfo->m_ListStatusSocket.m_start_sem);
        readingListFeedBack=true;        
      }
      char* pinfo=buffer+13;
      int len=strlen(pinfo);
      pinfo[len]='\n';

      pinfo[len+1]='\0';      
      tInfo->m_ListStatusSocket.m_se.Send(pinfo,len+1);
      char command2[256];
      if (-1==sscanf(pinfo,"%s",command2)) continue;
      string s_command2=command2;
      if (s_command2=="Over")
      {
        readingListFeedBack=false;
        FeiSocketSession invalid;
        tInfo->m_ListStatusSocket.m_se=invalid;
        sem_post(&tInfo->m_ListStatusSocket.m_finish_sem);        
      }      
    }
  }
}

void SendCommand(const char* cmd, FILE* fp,  pthread_mutex_t lock)
{
   pthread_mutex_lock(&lock);  
   fputs(cmd,fp);  
   fputc('\n',fp); 
   fflush(fp);
   pthread_mutex_unlock(&lock);  
}


int HostProcess(pid_t listenerID, FILE* fout, FILE* fin)
{
  CommandFeedBackThreadInfo cfbInfo;
  cfbInfo.m_fin=fin;
  pthread_t CommandFeedBackThread;
  int threadError = pthread_create(&CommandFeedBackThread,NULL,HostProcess_CommandFeedBackThread,&cfbInfo);

  pthread_t KeyBoardInputThread;
  pthread_mutex_t cmd_lock;
  pthread_mutex_init(&cmd_lock,NULL);  

  sem_init(&cfbInfo.m_CDStatusSocket.m_finish_sem,0,1);  
  sem_init(&cfbInfo.m_CDStatusSocket.m_start_sem,0,0);  

  sem_init(&cfbInfo.m_CDListSocket.m_finish_sem,0,0);  
  sem_init(&cfbInfo.m_CDListSocket.m_start_sem,0,0);  

  sem_init(&cfbInfo.m_ListStatusSocket.m_finish_sem,0,1);  
  sem_init(&cfbInfo.m_ListStatusSocket.m_start_sem,0,0); 

  sem_init(&cfbInfo.m_ListListsSocket.m_finish_sem,0,0);  
  sem_init(&cfbInfo.m_ListListsSocket.m_start_sem,0,0);  

  sem_init(&cfbInfo.m_ListSongsSocket.m_finish_sem,0,0);  
  sem_init(&cfbInfo.m_ListSongsSocket.m_start_sem,0,0);  
  
  FeiSocketSever server(PORT);
  if (!server.IsValid())
  {
    SendCommand("Quit",fout, cmd_lock);
    return -1;
  }

  while (1)
  {
    FeiSocketSession se=server.GetSession();
    if (se.IsValid())
    {
      char buffer[4096];
      char command[256];
      int len;
      len=se.Recieve(buffer,4096);
      if (len<=0) 
      {
        se.Close();
        break;
      }
      buffer[len]=0;
      char *pos;
      if ((pos=strchr(buffer, '\n')) != NULL)
        *pos = '\0';
      if (-1==sscanf(buffer,"%s",command)) continue;
      string s_command=command;
      
      if (s_command=="Shutdown" || s_command=="Reboot")
      {
        SendCommand("Quit",fout, cmd_lock);
        if (s_command=="Shutdown")
        {
          PlaySound("shutdown.mp3");
          system("sudo halt -p");
          return 0;
        }
        else if (s_command=="Reboot")
        {
          PlaySound("reboot.mp3");
          system("sudo reboot");
          return 0;
        }
      }
      else if (s_command=="ResetCDWatch")
      {
        if (cfbInfo.m_CDStatusSocket.m_se.IsValid())
        {
          se.Send("playing\n",8);
          cfbInfo.m_CDStatusSocket.m_resetSocket=true;
		  		SendCommand("CurTrack",fout, cmd_lock);
          sem_wait(&cfbInfo.m_CDStatusSocket.m_finish_sem);
          cfbInfo.m_CDStatusSocket.m_se=se;
          sem_post(&cfbInfo.m_CDStatusSocket.m_start_sem);
        }
        else
        {
          se.Send("notplaying\n",11);
        }
      }      
	  	else if (s_command=="ResetListWatch")
      {
        if (cfbInfo.m_ListStatusSocket.m_se.IsValid())
        {
          se.Send("playing\n",8);
          cfbInfo.m_ListStatusSocket.m_resetSocket=true;
		  		SendCommand("CurSong",fout, cmd_lock);
          sem_wait(&cfbInfo.m_ListStatusSocket.m_finish_sem);
          cfbInfo.m_ListStatusSocket.m_se=se;
          sem_post(&cfbInfo.m_ListStatusSocket.m_start_sem);
        }
        else
        {
          se.Send("notplaying\n",11);
        }
      }      
      else 
      {
        SendCommand(buffer,fout, cmd_lock);
        if (s_command=="PlayCD")
        {
          sem_wait(&cfbInfo.m_CDStatusSocket.m_finish_sem);
          cfbInfo.m_CDStatusSocket.m_resetSocket=false;
          cfbInfo.m_CDStatusSocket.m_se=se;
          sem_post(&cfbInfo.m_CDStatusSocket.m_start_sem);
        }
        else if (s_command=="ListCD")
        {
          cfbInfo.m_CDListSocket.m_se=se;
          sem_post(&cfbInfo.m_CDListSocket.m_start_sem);
          sem_wait(&cfbInfo.m_CDListSocket.m_finish_sem);
        } 
				else if (s_command=="PlayList")
        {
          sem_wait(&cfbInfo.m_ListStatusSocket.m_finish_sem);
          cfbInfo.m_ListStatusSocket.m_resetSocket=false;
          cfbInfo.m_ListStatusSocket.m_se=se;
          sem_post(&cfbInfo.m_ListStatusSocket.m_start_sem);
        }
				else if (s_command=="ListLists")
        {
          cfbInfo.m_ListListsSocket.m_se=se;
          sem_post(&cfbInfo.m_ListListsSocket.m_start_sem);
          sem_wait(&cfbInfo.m_ListListsSocket.m_finish_sem);
        } 
				else if (s_command=="ListSongs")
        {
          cfbInfo.m_ListSongsSocket.m_se=se;
          sem_post(&cfbInfo.m_ListSongsSocket.m_start_sem);
          sem_wait(&cfbInfo.m_ListSongsSocket.m_finish_sem);
        } 
       
      }
    }
  }
  pthread_mutex_destroy(&cmd_lock);  
  
  return 0;
}

#include "GetIP.h"

void PlayIP(const unsigned char ip[4])
{
  FILE *fout=0;
  FILE *fin=0;
  pid_t pid;
  pid=LaunchChild(fout,fin); 
  if (pid==0)
  {
    char strings[4][4];
	sprintf(strings[0],"%d",ip[0]);
	sprintf(strings[1],"%d",ip[1]);
	sprintf(strings[2],"%d",ip[2]);
	sprintf(strings[3],"%d",ip[3]);	
    execlp("/home/pi/omxbyteplayer","omxbyteplayer", strings[0],strings[1],strings[2],strings[3],0);
  }
  else
  {
    waitpid(pid,0,0);
  }  
}

int main()
{
  unsigned char ip[4];
  if (GetIP(ip))
	  PlayIP(ip);

  PlaySound("start.mp3");
  //return CommandListenerProcess(stdout,stdin);
  FILE *fout=0;
  FILE *fin=0;
  pid_t pid= LaunchChildSymm(fout,fin);

   if (pid==0)
   {
     return CommandListenerProcess(fout,fin);
   }
   else return HostProcess(pid, fout, fin);
}

