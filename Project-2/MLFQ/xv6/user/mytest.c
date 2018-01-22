
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"
#include "syscall.h"
#include "traps.h"
#include "pstat.h"



int main(int argc, char **argv){



int i=0;
struct pstat *ptr = (struct pstat *)malloc(sizeof(struct pstat));

if (ptr == NULL)
printf(1,"\nMALLOC failure\n");

if (getpinfo(ptr) == 0){
for (i=0;i<64;i++){
   printf(1,"\ngetpinfo success pid %d \n", ptr->pid[i]);
   printf(1,"\ngetpinfo success tot RUN  ticks for q 3 %d q 2 %d q 1 %d q 0 %d \n", ptr->ticks[i][3] , ptr->ticks[i][2],  ptr->ticks[i][1] , ptr->ticks[i][0]  );
   printf(1,"\ngetpinfo success tot WAIT ticks for q 3 %d q 2 %d q 1 %d q 0 %d \n", ptr->wait_ticks[i][3] , ptr->wait_ticks[i][2],  ptr->wait_ticks[i][1] , ptr->wait_ticks[i][0]  );
   printf(1, "\n******** END ******\n");
}
}else{

   printf(1,"\ngetpinfo failed\n");
}

free(ptr);

return 0;
}
