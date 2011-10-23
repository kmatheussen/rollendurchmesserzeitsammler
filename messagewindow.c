

static semaphore_t *messagewindow_semaphore;
static int xmessage_found=1;

static void* messagewindow_thread(void *arg){
  char command[10000];
  while(true){
    char *message=(char*)semaphore_wait(messagewindow_semaphore);

    if( ! xmessage_found)
      sprintf(command,"xmessage %s",message);
    else
      sprintf(command,"%s %s",WHICH_XMESSAGE,message);

    system(command);
  }
  return NULL;
}

static bool messagewindow_show_message(char *message){
  if(semaphore_is_waiting(messagewindow_semaphore)==false)
    return false;
  semaphore_signal_val(messagewindow_semaphore,message);
  return true;
}

static void init_messagewindow(void){
  pthread_t *thread;

  if(access(WHICH_XMESSAGE,X_OK)!=0){
    fprintf(stderr,"Warning, \"xmessage\" is not found or is not an executable. I will try to use the $PATH instead. Hopefully that'll work,");
    fprintf(stderr,"but you might not receive messages to the screen in case of warning.");
    xmessage_found=0;
  }

  messagewindow_semaphore=semaphore_create(0);

  thread=calloc(sizeof(pthread_t*),1);
  pthread_create(thread,NULL,messagewindow_thread,NULL);
}
