#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <string>
#include <sys/wait.h>

#include "FeiSocket.h"
#define PORT 30001

using namespace std;

pid_t LaunchChild (FILE* &masterWriteStream, FILE* &masterReadStream)
{
  pid_t pid;
  int mypipe[2];
  int mypipe2[2];
  
  pipe (mypipe);
  pipe (mypipe2);
  pid = fork ();
  if (pid == (pid_t) 0)
  {
    dup2(mypipe[0], STDIN_FILENO);
    dup2(mypipe2[1], STDOUT_FILENO);
    close(mypipe[0]);
    close(mypipe[1]);
  }
  else
  {
    close(mypipe[0]);
    close(mypipe2[1]);
    masterWriteStream= fdopen (mypipe[1], "w");    
    setbuf(masterWriteStream,(char*)0);
    masterReadStream= fdopen(mypipe2[0],"r");
    setbuf(masterReadStream,(char*)0);
  }  
  
  return pid;

}

int main()
{
  //socket
  FeiSocketSever server(PORT);
  if (!server.IsValid()) return -1;

  //process com
  char buffer[1024];
  FILE *fout=0;
  FILE *fin=0;
  pid_t pid;

  while (1)
  {
    FeiSocketSession se=server.GetSession();
    if (!se.IsValid()) continue;
    printf("New connection established.\n");
  
    while (1)
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

      printf("Command recieved: %s\n", buffer);
      sscanf(buffer,"%s",command);
      string s_command=command;

      if (s_command=="Reboot") system("sudo reboot");
      else if (s_command=="Shutdown") system("sudo halt -p");
     
      else if (s_command=="PlayURL" || s_command=="Quit" || s_command=="Stop")
      {
        if (fout) 
        {
          int status;
          pid_t result = waitpid(pid, &status, WNOHANG);
          if (result == 0)
          {
            printf("Child-process is active.\n");
            fputc('q',fout);
            wait(0);
          }
          else printf("Child-process is not active.\n");
          fclose(fin);
          fclose(fout);
          fin=0;
          fout=0;
        } 
        if (s_command=="Quit") 
        {
          se.Close();
          printf("Quit..\n");
          return 0;
        }
        else if (s_command=="PlayURL")
        {
          char* p_url=buffer+8;
          pid=LaunchChild(fout,fin);    
          if (pid==0)
          {
            execlp("omxplayer","omxplayer", p_url,0);
          }
        }
      }
      else if (s_command=="VolDown" || s_command=="VolUp")
      {
        if (fout) 
        {
          int status;
          pid_t result = waitpid(pid, &status, WNOHANG);
          if (result == 0)
          {
            if (s_command=="VolDown")
              fputc('-',fout);
            else 
              fputc('+',fout);
          }
         }
      }
      
    }
    printf("Disconnected.\n");    
  }

  return 0;
}

