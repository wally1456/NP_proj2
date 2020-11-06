#include<unistd.h>
#include<stdlib.h>
#include<iostream>
#include<string>
#include<string.h>
#include<vector>
#include<wait.h>
#include<fcntl.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>
using namespace std;

void split_input(string str,vector<string> &tuple){
	char *cstr = new char[str.length() + 1];
	strcpy(cstr, str.c_str());
	char *p = strtok(cstr, " ");
	while (p != 0){
		tuple.push_back(p);
		p = strtok(NULL, " ");
	}
}
void write_to_file(char *filename){
	int fd;
	mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	fd = creat(filename, mode);
	if (fd == -1)
		cerr << "create file error.\n";
	dup2(fd,STDOUT_FILENO);
}
void close_pipe(int pipeline[][2],int pipe_count){
	if (pipe_count != 0){
		close(pipeline[pipe_count-1][0]);
		close(pipeline[pipe_count-1][1]);
	}
	close(pipeline[pipe_count][0]);
	close(pipeline[pipe_count][1]);
}
void childHandler(int signo){ 
	int status; 
	while (waitpid(-1, &status, WNOHANG) > 0) { 
		//do nothing 
	} 
}
void excute_cmd(vector<string> s_input,vector<vector<int>> &number_pipe,int cmd_count){
	signal(SIGCHLD, childHandler);
	int i = 0;
	vector<string> tmp_input;
	int pipeline[1000][2];//maximum command = 1000
	int pipe_count = -1;
	vector<pid_t> pidTable;
	bool is_number_pipe = 0;
	bool have_number_pipe = 0;
	bool ordinary_pipe = 0;
	int number;

	if (!number_pipe[cmd_count%1000].empty()){
		have_number_pipe = 1;
	}
	while(i<s_input.size()){
		if (s_input[i]=="|" || s_input[i]==">" || i == s_input.size()-1){
			if(i == s_input.size()-1 && (s_input[i][0]=='|' || s_input[i][0]=='!')){
				if(s_input[i][0]=='!')
					ordinary_pipe = 1;
				s_input[i].erase(0,1);
				number = stoi(s_input[i].c_str());
				is_number_pipe=1;
			}
			else if(i == s_input.size()-1)
				tmp_input.push_back(s_input[i]);      

			pipe_count += 1 ;

			if(pipe(pipeline[pipe_count])<0)
				cerr << "create pipe error" << std::endl;

			pid_t pid;
			while((pid = fork())<0){
				usleep(1000);
			}
			pidTable.push_back(pid);
			if(pid==0){//child
				if (pipe_count != 0)
					dup2(pipeline[pipe_count-1][0],STDIN_FILENO); 
				else if (have_number_pipe){
					dup2(number_pipe[cmd_count%1000][0],STDIN_FILENO);
					close(number_pipe[cmd_count%1000][0]);
					close(number_pipe[cmd_count%1000][1]);
				}
				if (i != s_input.size()-1 )
					dup2(pipeline[pipe_count][1],STDOUT_FILENO);
				else if(is_number_pipe){
					int pipe_num;
					if (number_pipe[(cmd_count+number)%1000].empty())
						pipe_num = pipeline[pipe_count][1];
					else
						pipe_num = number_pipe[(cmd_count+number)%1000][1];                          
					dup2(pipe_num,STDOUT_FILENO);
					if (ordinary_pipe)
						dup2(pipe_num,STDERR_FILENO);    
				}
				if(s_input[i]==">"){
					write_to_file((char*)(s_input[i+1].c_str()));    
				}                                      
				close_pipe(pipeline,pipe_count);

				char* arg[tmp_input.size()+1];
				for(int j=0;j<tmp_input.size();j++){
					arg[j] = (char*)(tmp_input[j].c_str());
				}

				arg[tmp_input.size()] = NULL;
				execvp(arg[0],arg);
				cerr << arg[0] << " is BAKANONO" << endl; //unknow commend
				exit(1);
			}
			else{//paraent
				if(s_input[i]==">")
					i++;
				if (pipe_count != 0){
					close(pipeline[pipe_count-1][0]);
					close(pipeline[pipe_count-1][1]);
				} 
				if (i == s_input.size()-1){
					if (!is_number_pipe){
						close(pipeline[pipe_count][0]);
						close(pipeline[pipe_count][1]);
					}
					else{                    
						if (number_pipe[(cmd_count+number)%1000].empty()){
							number_pipe[(cmd_count+number)%1000].push_back(pipeline[pipe_count][0]);
							number_pipe[(cmd_count+number)%1000].push_back(pipeline[pipe_count][1]);  
						}
						else{
							close(pipeline[pipe_count][0]);
							close(pipeline[pipe_count][1]);                            
						}    
					}
				}
				if(have_number_pipe){
					while(!number_pipe[cmd_count%1000].empty()){
						int fd = number_pipe[cmd_count%1000].back();
						close(fd);
						number_pipe[cmd_count%1000].pop_back();
					}
				}
			}
			tmp_input.clear();
		}
		else{
			tmp_input.push_back(s_input[i]);  
		} 
		i++;                 
	}
	for(int i=0;i<pidTable.size();i++){
		if(is_number_pipe && (i==pidTable.size()-1))
			continue;
		int status;
		waitpid(pidTable[i],&status,0);  
	}
}

int main(int argc, char** argv){
	string input;
	//setenv("PATH","./,bin/",1);
	setenv("PATH","bin",1);
	vector<vector<int>> number_pipe(1000);
	int cmd_count = 0;


	int  listenfd, connfd;
	struct sockaddr_in  servaddr;
	char  buff[4096];
	int  n;

	if( (listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1 ){
		printf("create socket error: %s(errno: %d)\n",strerror(errno),errno);
		return 0;
	}

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(atoi(argv[1]));

	if( bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1){
		printf("bind socket error: %s(errno: %d)\n",strerror(errno),errno);
		return 0;
	}

	if( listen(listenfd, 10) == -1){
		printf("listen socket error: %s(errno: %d)\n",strerror(errno),errno);
		return 0;
	}
    while(1){
        if( (connfd = accept(listenfd, (struct sockaddr*)NULL, NULL)) == -1){
            printf("accept socket error: %s(errno: %d)",strerror(errno),errno);
            
            continue;
        }
        if (connfd){
            dup2(connfd,STDIN_FILENO); 
            dup2(connfd,STDOUT_FILENO);
            dup2(connfd,STDERR_FILENO);
            close(connfd);
            break;
        }   
    }
    close(listenfd);

	while(1){
		vector<string> s_input;        
		cout << "% ";
		getline(cin,input);

        if(input.size()!=0)
            if(input[input.size()-1] == '\r')
                input[input.size()-1] = ' ';

        for(int i=0;i< input.size();i++)
            cout << i << "\t" <<  input[i] << endl;
		split_input(input,s_input);
		if (s_input.size()<1)
			continue;       

		if (s_input[0]=="exit" || s_input[0]=="EOF")
			break;

		else
			cmd_count += 1;
		if (s_input[0]=="printenv"){
			cerr << getenv(s_input[1].c_str()) << endl;
			continue;
		}
		else if (s_input[0]=="setenv"){
			setenv(s_input[1].c_str(),s_input[2].c_str(),1);
			continue;
		}
		excute_cmd(s_input,number_pipe,cmd_count);
	}
}
