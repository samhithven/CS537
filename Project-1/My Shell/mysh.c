#include<stdio.h>
#include<fcntl.h>
#include<unistd.h>
#include<sys/wait.h>
#include<string.h>
#include<stdlib.h>
#include<ctype.h>
#include<signal.h>
#include<errno.h>

#define MAX_LENGTH 128
#define EXITCMD "exit"
#define PWD "pwd"
#define CD "cd"
#define DELIM " \t"

struct node {
	char* content;
	struct node* next;
};
int node_list_length = 0;
int bg_process_list[25] = {0};
int background_process = 0;
int background = 0;
int count = 0;


struct node* head = NULL;
char* input_redirection = NULL;
char* output_redirection = NULL;
struct node* output_pipe = NULL;

void kill_background_process() {
	int i=0;
	while(i<25) {
		if(bg_process_list[i]!=0) {
			kill(bg_process_list[i],SIGKILL);
		}
		i++;
	}
}

void handle_sigchld(int sig) {
	int pid =0, temp;
	while ((temp = waitpid((pid_t)(-1), 0, WNOHANG)) > 0) {
		pid = temp;
	}
	temp = 0;
	while(temp<25) {
		if(pid!=0&&bg_process_list[temp]==pid) {
			bg_process_list[temp] = 0;
			break;
		}
		temp++;
	}
}	

void clean_up(int type) {

        struct node* temp = head;
        struct node* temp1;
	input_redirection = NULL;
	output_redirection = NULL;
        output_pipe = NULL;
	background = 0;
	head = NULL;
	count = 0;
	//printf("Clean up started\n");
	while(temp!=NULL) {
        	temp1 = temp;
		temp = temp->next;
		free(temp1);
	}
	node_list_length = 0;
	
	//If type == 0 simple clean up 
	//Else deep clean up i.e killing of all background process
	if(type==1) {
		kill_background_process();
	}
	//printf("Clean up ended\n");
}

void print_error(int type) {
        char error_message[30] = "An error has occurred\n";
        if(write(STDERR_FILENO,error_message,strlen(error_message))==-1) {
		perror("write failure");
	}
	clean_up(type);
}

int is_empty(char *line) {
	char *temp = line;
	int i = 0; 
	int n = strlen(line);
	while(i<n) {
		if(temp[i]!=' ') {
			return 0;
		}
		i++;
	}
	return 1;
}


int get_input(char* line) {
	if(fgets(line,1024,stdin) == NULL) {
		print_error(1);
	}
	if(strlen(line)==1&&line[0]=='\n') {
		return 2;
	} else if(strlen(line)>128) {
		print_error(0);
		return 1;
	}
	line[strlen(line)-1] = '\0';
	if(is_empty(line)) {
		return 2;
	}	
	return 0;
}

int inbuilt_commands(struct node* list) {
	char* line = list->content;
	char* temp;
	int ret;
	//printf("Inbuilt Command token:\"%s\"\n",line);
	if(strcmp(line,EXITCMD)==0) {
		clean_up(1);
		exit(0);
	} else if(strcmp(line,PWD)==0) {
		temp = malloc(1024*sizeof(char));
		if(temp==NULL) {
			print_error(1);
			return 0;
		}
		if(list->next!=NULL) {
			print_error(1);
			return 0;
		}
		if(getcwd(temp,1024*sizeof(char))==NULL) {
			print_error(1);
			free(temp);
			return 0;
		}
		printf("%s\n",temp); 
		free(temp);
		return 1;
	} else if(strcmp(line,CD)==0) {
		if(list->next==NULL) {
			temp = getenv("HOME");
			if(temp==NULL) {
				print_error(1);
				return 0;
			}
			ret = chdir(temp);
		} else {
			ret = chdir(list->next->content);
		}
		if(ret==0) {
			return 1;
		} else {
			print_error(1);
			return 0;
		}
	}
	return 0;
}

struct node* create_node(char* input) {
	struct node* temp = malloc(sizeof(struct node));
	if(temp==NULL) {
		print_error(0);
	}
	temp->next=NULL;
	temp->content = input;	
	//printf("Node created\n");
	return temp;
}

void add_at_end(struct node* new_node) {
	struct node* temp = head;
	if(head==NULL) {
		count = 1;
		head = new_node;
		return;
	}
	while(temp->next!=NULL) {
		count++;
		temp = temp->next;
	}
	temp->next = new_node;
	//printf("Node added at the end");
}

int is_valid_file_name(char *text) {
	int i; 
	char bad_chars[] = "!@%^*~|";
	if(text==NULL) {
		return 0;
	} else if(is_empty(text)==1) {
		return 0;
	} else {
		for (i = 0; i < strlen(bad_chars); ++i) {
    			if (strchr(text, bad_chars[i]) != NULL) {
    				return 0;
			}
		}			
	}
	return 1;
}

int identify_spl_case(struct node* head) {
	struct node* list = head;
	char *input;
	int i = 0;
	while(list!=NULL) {
		i++;
		input = list->content;
		//Check if it is input redirection
		if(strcmp(input,"<")==0) {
			if(list->next!=NULL && is_valid_file_name(list->next->content) && (list->next->next==NULL || strcmp(list->next->next->content,">")==0 || strcmp(list->next->next->content,"&")==0)) {
				input_redirection  = list->next->content;
				if(node_list_length==0) {
					node_list_length = i-1;
				}
			} else {
				print_error(0);
				return 1;
			}
		} else if(strcmp(input,">")==0) {
			if(list->next!=NULL && is_valid_file_name(list->next->content) && (list->next->next==NULL || strcmp(list->next->next->content,"<")==0 || strcmp(list->next->next->content,"&")==0)) {
				output_redirection = list->next->content;
				if(node_list_length==0) {
					node_list_length = i-1;
				}
			} else {
				print_error(0);
				return 1;
			}
		} else if(strcmp(input,"|")==0) {
			if(list->next!=NULL) {
				output_pipe = list->next;
				if(node_list_length==0) {
                                        node_list_length = i-1;
                                }
			} else {
				print_error(0);
				return 1;
			}
		} else if(strcmp(input,"&")==0) {
			if(list->next==NULL || strcmp(list->next->content,"<")==0 || strcmp(list->next->content,">")==0) {
				i--;
				background = 1;
			} else {
				print_error(0);
				return 1;
			}
		} 
		list = list->next;
	}
	if(node_list_length==0) {
		node_list_length = i;
	}
	return 0;
}


void obtain_token(char *input) {
	char* token =  strtok(input,DELIM);
	while(token!=NULL) {
		//printf("Token:\"%s\"\n",token);
		add_at_end(create_node(token));
		token =  strtok(NULL,DELIM);
	}
	//printf("Tokenizing done\n");
}

void traverse_list(struct node* list) {
	printf("List traversal started\n");
	while(list!=NULL) {
		printf("\"%s\"\n",list->content);
		list = list->next;
	}
	printf("List traversal ended\n");
}

int execute_commands(struct node* list) {
	long i;
	int fd0,fd1;
	struct sigaction sa;
	char* args[node_list_length+1]; 
	char* args1[count-node_list_length];	
	int pipes[2];
	int prev_process;
	int ret_val;
	int temp;
	
	if(output_pipe==NULL) {
		//printf("Simple case execution reached\n");
		fflush(stdout);
		ret_val = fork();
		if(ret_val==0) {
			if(input_redirection!=NULL) {
				//printf("Input Redirection:\"%s\"\n",input_redirection);
				fd0 = open(input_redirection,O_RDONLY);
				//Handle error case
				if(fd0==-1) {
					print_error(0);
					exit(0);
				}
				dup2(fd0,STDIN_FILENO);
				close(fd0);	
			}
			if(output_redirection!=NULL) {
				//printf("Output Redirection:\"%s\"\n",output_redirection);
				fd1 = open(output_redirection,O_CREAT|O_WRONLY|O_TRUNC,0644);
				//Handle error case
				if(fd1==-1) {
					print_error(0);
					exit(0);
				}
				dup2(fd1,STDOUT_FILENO);
				close(fd1);
			}
			i=0;
			while(i<node_list_length) {
				//Identify the end of the command token
				args[i] = strdup(list->content);
				i++;
				list = list->next;
			}
			args[i] = NULL;
			execvp(args[0],args);
			print_error(0);	
			exit(0);
		} else if (ret_val > 0 ) {
			if(background==1) {
				sa.sa_handler = &handle_sigchld;
				sigemptyset(&sa.sa_mask);
				sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
				if (sigaction(SIGCHLD, &sa, 0) == -1) {
					print_error(0);
					return 0;
				}
				if(background_process<=25) {
					//bg_process_list[background_process] = ret_val;	
					temp = 0;
					while(temp<25) {
						if(bg_process_list[temp]==0) {
							bg_process_list[temp] = ret_val;
							break;
						}
						temp++;
					}				
					background_process++;
				} else {
					print_error(0);
					return 0;
				}	
			} else {
				wait(NULL);
				fflush(stdout);
			}
			return 1;
		} else {
			print_error(0);
		}	
	} else {
		//Implementing the pipe execution here
		fflush(stdout);
		if(pipe(pipes)!=0) {
			print_error(0);
			return 0;
		}
		ret_val = fork();
		if(ret_val==0) {
			close(STDOUT_FILENO);
			close(pipes[0]);
			dup2(pipes[1],STDOUT_FILENO);
			i=0;
                        while(i<node_list_length) {
                                args[i] = strdup(list->content);
                                i++;
                                list = list->next;
                        }
			args[i] = NULL;
                        execvp(args[0],args);
                        print_error(0);
                        exit(0);
		} else if(ret_val<0) {
			print_error(0);
			return 0;
		}
		prev_process = ret_val;
		ret_val = fork();
		if(ret_val==0) {
			close(STDIN_FILENO);
			close(pipes[1]);
			dup2(pipes[0],STDIN_FILENO);
			i = 0;
			while(output_pipe!=NULL) {
				args1[i] = strdup(output_pipe->content);
				i++;
				output_pipe = output_pipe->next;
			}
			args1[i] = NULL;
			execvp(args1[0],args1);
			kill(prev_process,SIGKILL);
			print_error(0);
			exit(0);
		} 
		if(ret_val>0) {
			close(pipes[0]);
			close(pipes[1]);
			waitpid(ret_val,NULL,0);
			fflush(stdout);
			kill(prev_process,SIGKILL);
			return 1;
		}
		print_error(0);
	}
	return 0;		
}

int 
main(int argc, char *argv[]) {
	unsigned long count = 1;
	char input[1024];
	int flag;
	if(argc>1) {
		print_error(1);
		exit(1);
	}	
	while(1) {
		printf("mysh (%ld)> ",count);
		flag = get_input(input);
		if(flag==1) {
			count++;
			clean_up(0);
			continue;
		} else if(flag==2) {
			clean_up(0);
			continue;
		}
		obtain_token(input);
		//traverse_list(head);
		flag = inbuilt_commands(head);
		if(flag==0) {
			if(identify_spl_case(head)==1) {
				count++;
				clean_up(0);
				continue;
			}
			flag = execute_commands(head);
		}
		if(flag==1) {
			count++; 
		} else {
			//printf("Input:%s\n",input);
		}
		clean_up(0);	
	}
}

