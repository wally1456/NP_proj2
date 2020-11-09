#include<unistd.h>
#include<stdlib.h>
#include<iostream>
#include<string>
#include<string.h>
#include<vector>
#include<wait.h>
#include<fcntl.h>
#include<arpa/inet.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>

using namespace std;
#define	QLEN	     30	/* maximum connection queue length */
#define	BUFSIZE	     4096

struct user
{
    int fd;
    string env;
    int ID;
    string name;
    char ip[INET6_ADDRSTRLEN];
    int port;
    int cmd_count;    
    vector<int> number_pipe[1000];
    vector<int> user_pipe[QLEN+1];
};

int		passiveTCP(const string service, int qlen);
int		client_cmd(vector<user> &user_table, int num);
int     passivesock( string service, string protocol, int	qlen );

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
void welcome_msg(){
    cerr << "***************************************\n** Welcome to the information server **\n**************************************\n";
}
int ID_find_index(int ID, vector<user> &user_table){
    for(int i=0;i<user_table.size();i++)
        if(user_table[i].ID==ID)
            return i;
    return -1;
}
void childHandler(int signo){ 
    int status; 
    while (waitpid(-1, &status, WNOHANG) > 0) { 
            //do nothing 
        } 
    }
void set_new_user(user &new_user,int ssock,struct sockaddr_in fsin,bool ID_table[QLEN+1]){
    new_user.fd=ssock;
    new_user.env="bin:.";
    for(int i=1;i<QLEN+1;i++)
        if(!ID_table[i]){
            new_user.ID=i;
            break;
        }
    struct sockaddr_in *s = (struct sockaddr_in *)&fsin;
    int port = ntohs(s->sin_port);
    inet_ntop(AF_INET, &s->sin_addr, new_user.ip, sizeof new_user.ip);
    new_user.port = port;
    new_user.name="(no name)";
    new_user.cmd_count=0;
}
void Broadcast(string msg,vector<user> &user_table){
    for(int i=0;i<user_table.size();i++){
        dup2(user_table[i].fd,STDERR_FILENO);
        cerr << msg << endl;
    }
}
void new_connect_action(user new_user,vector<user> &user_table){
    dup2(new_user.fd,STDERR_FILENO);
    welcome_msg();
    string Broadcast_msg = "*** User '(no name)' entered from "+ string(new_user.ip) +":" + to_string(new_user.port) +". *** ";
    Broadcast(Broadcast_msg,user_table);
    dup2(new_user.fd,STDERR_FILENO);
    cerr << "% ";
}
void clear_fd(){
    int fd_RD = open("/dev/null", O_RDONLY);
    int fd_WR = open("/dev/null", O_WRONLY);    
    dup2(fd_RD, STDIN_FILENO);
    dup2(fd_WR, STDOUT_FILENO);   
    dup2(fd_WR, STDERR_FILENO); 
    close(fd_RD);
    close(fd_WR);
}
void who(user now_user,vector<user> &user_table){
    dup2(now_user.fd,STDERR_FILENO);
    cerr << "<ID>\t<nickname>\t<IP:port>\t<indicate me>\n";
    for(int i=0;i<user_table.size();i++){
        user u = user_table[i];
        cerr << u.ID << "\t" <<u.name << "\t" << string(u.ip) << ":" << to_string(u.port) << "\t";
        if (u.ID == now_user.ID)
            cerr << "<- me";
        cerr << endl;
    }
}
void tell(string source_name, int traget_ID, string msg,vector<user> &user_table){
    int i = ID_find_index(traget_ID,user_table);
    if(i == -1){
        cerr <<"*** Error: user "<< traget_ID <<" does not exist yet. ***\n";
        return;
    }
    dup2(user_table[i].fd,STDERR_FILENO);
    cerr << "*** " << source_name << " told you ***: " << msg << endl;
}
void yell(string source_name, string msg,vector<user> &user_table){
    string Broadcast_msg ="*** "+ source_name + " yelled ***: " +msg;
    Broadcast(Broadcast_msg,user_table);
}
void name(int user_ID,string new_name,vector<user> &user_table){
    for(int i=0;i<user_table.size();i++)
        if(user_table[i].name==new_name){
            cerr << "*** User '"<< new_name <<"' already exists. ***\n";
            return;
        }
    int i = ID_find_index(user_ID,user_table);
    user_table[i].name = new_name;

    string Broadcast_msg ="*** User from "+ string(user_table[i].ip) +":" + to_string(user_table[i].port) +" is named '"+ new_name +"'. ***";
    Broadcast(Broadcast_msg,user_table);
}
void excute_cmd(vector<string> s_input,vector<user> &user_table, int user_num){
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
    int cmd_count = user_table[user_num].cmd_count;

    if (!user_table[user_num].number_pipe[cmd_count%1000].empty()){
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

            string cmd ="";
            int user_pipe_in = -1;
            string in_msg = "";
            string in_error = "";
            int user_pipe_out = -1;
            string out_msg = "";
            string out_error = "";
            for(int j=0;j<tmp_input.size();j++)
                cmd += tmp_input[j]+" ";
            cmd.pop_back();


            for(int j=0;j<tmp_input.size();j++){//user pipe
                if (tmp_input[j][0] == '<'){
                    tmp_input[j].erase(0,1);
                    int source_num = ID_find_index(stoi(tmp_input[j]),user_table);
                    if(!user_table[user_num].user_pipe[stoi(tmp_input[j])].empty()){
                        user_pipe_in = stoi(tmp_input[j]);
                        in_msg +="*** "+user_table[user_num].name+" (#"+ to_string(user_table[user_num].ID)+") just received from "+user_table[source_num].name+" (#"+ to_string(user_table[source_num].ID)+") by '"+cmd+"' ***";
                    }                       
                    else
                        in_error += "*** Error: the pipe " + tmp_input[j] + "->" + to_string(user_table[user_num].ID) +" does not exist yet. ***\n";
                    tmp_input.erase(tmp_input.begin()+j);
                }
                else if(tmp_input[j][0] == '>' && tmp_input[j].size()!=1){
                    tmp_input[j].erase(0,1);
                    int target_num = ID_find_index(stoi(tmp_input[j]),user_table);
                    if(ID_find_index(stoi(tmp_input[j]),user_table)==-1){
                        cerr << "*** Error: user #"+ tmp_input[j] +" does not exist yet. ***";
                    }
                    else if(user_table[target_num].user_pipe[user_table[user_num].ID].empty()){
                        user_pipe_out = stoi(tmp_input[j]);
                        out_msg +="*** "+user_table[user_num].name+" (#"+ to_string(user_table[user_num].ID)+") just piped '"+cmd+"' to "+user_table[target_num].name+" (#"+ to_string(user_table[target_num].ID)+") ***";
                    }
                    else
                        out_error += "*** Error: the pipe " + to_string(user_table[user_num].ID) + "->" + tmp_input[j] +" already exist. ***\n";                    
                    tmp_input.erase(tmp_input.begin()+j);
                }
            }
            if(in_msg!="")
                Broadcast(in_msg,user_table);
            else if(in_error!=""){
                cerr << in_error;                
                int fd_RD = open("/dev/null", O_RDONLY);   
                dup2(fd_RD, STDIN_FILENO); 
                close(fd_RD);
            }
            if(out_msg!="")
                Broadcast(out_msg,user_table);
            else if(out_error!=""){
                cerr << out_error;                
                int fd_WR = open("/dev/null", O_WRONLY);    
                dup2(fd_WR, STDOUT_FILENO);   
                dup2(fd_WR, STDERR_FILENO); 
                close(fd_WR);
            }
            

            pid_t pid;
            while((pid = fork())<0){
                usleep(1000);
            }
            pidTable.push_back(pid);
            if(pid==0){//child
                if (pipe_count != 0)
                    dup2(pipeline[pipe_count-1][0],STDIN_FILENO); 
                else if (have_number_pipe){
                    dup2(user_table[user_num].number_pipe[cmd_count%1000][0],STDIN_FILENO);
                    close(user_table[user_num].number_pipe[cmd_count%1000][0]);
                    close(user_table[user_num].number_pipe[cmd_count%1000][1]);
                }
                else if (user_pipe_in!=-1){
                    dup2(user_table[user_num].user_pipe[user_pipe_in][0],STDIN_FILENO);
                    close(user_table[user_num].user_pipe[user_pipe_in][0]);
                    close(user_table[user_num].user_pipe[user_pipe_in][1]);
                }

                if (i != s_input.size()-1 )
                    dup2(pipeline[pipe_count][1],STDOUT_FILENO);
                else if(is_number_pipe){
                    int pipe_num;
                    if (user_table[user_num].number_pipe[(cmd_count+number)%1000].empty())
                        pipe_num = pipeline[pipe_count][1];
                    else
                        pipe_num = user_table[user_num].number_pipe[(cmd_count+number)%1000][1];                          
                    dup2(pipe_num,STDOUT_FILENO);
                    if (ordinary_pipe)
                        dup2(pipe_num,STDERR_FILENO);    
                }
                else if (user_pipe_out!=-1){
                    dup2(pipeline[pipe_count][1],STDOUT_FILENO);
                }

                if(s_input[i]==">"){
                    write_to_file((char*)(s_input[i+1].c_str()));    
                }                      
                close_pipe(pipeline,pipe_count);

                for (int j=0;j<user_table.size();j++)
                    close(user_table[j].fd);

                char* arg[tmp_input.size()+1];
                for(int j=0;j<tmp_input.size();j++){
                    arg[j] = (char*)(tmp_input[j].c_str());
                }
                arg[tmp_input.size()] = NULL;
                execvp(arg[0],arg);
                cout << arg[0] << " is BAKANONO" << endl; //unknow commend
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

                    if (is_number_pipe){
                        if (user_table[user_num].number_pipe[(cmd_count+number)%1000].empty()){
                            user_table[user_num].number_pipe[(cmd_count+number)%1000].push_back(pipeline[pipe_count][0]);
                            user_table[user_num].number_pipe[(cmd_count+number)%1000].push_back(pipeline[pipe_count][1]);  
                        }
                        else{
                            close(pipeline[pipe_count][0]);
                            close(pipeline[pipe_count][1]);                            
                        }                 
                    }
                    else if(user_pipe_out!=-1){
                        user_table[ID_find_index(user_pipe_out,user_table)].user_pipe[user_table[user_num].ID].push_back(pipeline[pipe_count][0]);
                        user_table[ID_find_index(user_pipe_out,user_table)].user_pipe[user_table[user_num].ID].push_back(pipeline[pipe_count][1]);
                    }
                    else{                    
                        close(pipeline[pipe_count][0]);
                        close(pipeline[pipe_count][1]);
                    }
                }
                if(have_number_pipe){
                    while(!user_table[user_num].number_pipe[cmd_count%1000].empty()){
                        int fd = user_table[user_num].number_pipe[cmd_count%1000].back();
                        close(fd);
                        user_table[user_num].number_pipe[cmd_count%1000].pop_back();
                    }
                }
                else if(user_pipe_in!=-1){
                    while(!user_table[user_num].user_pipe[user_pipe_in].empty()){
                        int fd = user_table[user_num].user_pipe[user_pipe_in].back();
                        close(fd);
                        user_table[user_num].user_pipe[user_pipe_in].pop_back();
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
void user_logout(vector<user> &user_table, int user_num, bool ID_table[QLEN+1], fd_set	&afds){
    user now_user = user_table[user_num];
    string broadcast_msg ="*** User "+ now_user.name +" left. ***";
    close(now_user.fd);
	FD_CLR(now_user.fd, &afds);
    ID_table[now_user.ID]=0;
    for(int i=1;i<QLEN+1;i++)
        while(!now_user.user_pipe[i].empty()){
            int fd = now_user.user_pipe[i].back();
            close(fd);
            now_user.user_pipe[i].pop_back();
        }
    for(int i=0;i<user_table.size();i++)
        while(!user_table[i].user_pipe[now_user.ID].empty()){
            int fd = user_table[i].user_pipe[now_user.ID].back();
            close(fd);
            user_table[i].user_pipe[now_user.ID].pop_back();
        }
    user_table.erase(user_table.begin()+ user_num);
    Broadcast(broadcast_msg,user_table);
    clear_fd();
}

int  main(int argc, char *argv[])
{	
    string service = argv[1];	/* service name or port number	*/
	struct sockaddr_in fsin;	/* the from address of a client	*/
	int	msock;		/* master server socket	*/
	fd_set	rfds;		/* read file descriptor set	*/
	fd_set	afds;		/* active file descriptor set	*/
	socklen_t	alen;		/* from-address length	*/
	int	fd, nfds;
    bool ID_table[QLEN+1]={0};

    vector<user> user_table;

    msock = passiveTCP(service, QLEN);

	nfds = getdtablesize();
	FD_ZERO(&afds);
	FD_SET(msock, &afds);
	while (1) {
		memcpy(&rfds, &afds, sizeof(rfds));
		if (select(nfds, &rfds, (fd_set *)0, (fd_set *)0,(struct timeval *)0) < 0){
            if(errno == EINTR)
                continue;
			cout <<"select: " << strerror(errno) << "\n" ; 
        }
        if (FD_ISSET(msock, &rfds)) {
            int	ssock;
            alen = sizeof(fsin);
            ssock = accept(msock, (struct sockaddr *)&fsin,&alen);
            if (ssock < 0)
                cout <<"accept: " << strerror(errno) << "\n" ;
            user new_user ={};
            set_new_user(new_user,ssock,fsin,ID_table);
            ID_table[new_user.ID]=1;
            user_table.insert(user_table.begin()+new_user.ID-1,new_user);
            FD_SET(ssock, &afds);
            new_connect_action(new_user,user_table);
        }
		for (int i = 0; i<user_table.size(); i++){
            fd = user_table[i].fd;
			if (fd != msock && FD_ISSET(fd, &rfds))
				if (client_cmd(user_table,i) == 0) 
                    user_logout(user_table,i,ID_table,afds);
        }
	}
}

int client_cmd(vector<user> &user_table, int user_num)
{
	char buf[BUFSIZE]={'\0'};
	int	cc;
    int fd = user_table[user_num].fd;
	cc = read(fd, buf, sizeof buf);
    if (cc ==0)
        return cc;

    string input(buf);
    vector<string> s_input;

    for(int i=input.size()-1;i>=0;i--){
        if(input[i] == '\r' || input[i] == '\n')
            input[i] = ' ';
        else
            break;
    }
    split_input(input,s_input);

    dup2(fd,STDIN_FILENO); 
    dup2(fd,STDOUT_FILENO);
    dup2(fd,STDERR_FILENO);

    if (s_input.size()<1){
        cerr << "% ";
        return cc;
    }
    if(s_input[0]=="exit")
        return 0;

    bool break_flag=1;        
    if (s_input[0]=="who")
        who(user_table[user_num],user_table);
    else if (s_input[0]=="name"){
        string str = "";
        for(int i=1;i<s_input.size();i++)
            str += s_input[i]+" ";
        str.pop_back();
        name(user_table[user_num].ID,str,user_table);
    }
    else if (s_input[0]=="tell"){
        string str = "";
        for(int i=2;i<s_input.size();i++)
            str += s_input[i]+" ";
        str.pop_back();
        tell(user_table[user_num].name,stoi(s_input[1]),str,user_table);
    }
    else if (s_input[0]=="yell"){
        string str = "";
        for(int i=1;i<s_input.size();i++)
            str += s_input[i]+" ";
        str.pop_back();
        yell(user_table[user_num].name,str,user_table);
    }
    else
        break_flag=0;

    user_table[user_num].cmd_count += 1;

    if(break_flag){
        cerr << "% ";
        return cc;
    }

    setenv("PATH","bin:.",1);
    if (s_input[0]=="printenv"){
        cerr << getenv(user_table[user_num].env.c_str()) << endl;
        return cc;
    }
    else if (s_input[0]=="setenv"){
        setenv(s_input[1].c_str(),s_input[2].c_str(),1);
        user_table[user_num].env = s_input[2].c_str();
        return cc;
    }
    excute_cmd(s_input,user_table,user_num);
    dup2(fd,STDERR_FILENO);
    cerr << "% ";
    return cc;
}

int passiveTCP(const string service,int qlen )
{
	return passivesock(service, "tcp", qlen);
}
int passivesock(string service, string protocol, int	qlen )
{
	struct servent    *pse;	/* pointer to service information entry	*/
	struct protoent *ppe;	/* pointer to protocol information entry*/
	struct sockaddr_in sin;	/* an Internet endpoint address	*/
	int	s, type;	/* socket descriptor and socket type	*/

	bzero((char *)&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
    /* Map service name to port number */
	if ( pse = getservbyname(service.c_str(), protocol.c_str()) )
		sin.sin_port = htons(ntohs((u_short)pse->s_port));
	else if ( (sin.sin_port = htons((u_short)atoi(service.c_str()))) == 0 )
		cout <<"can't get \""<< service.c_str() <<"\" service entry\n";

    /* Map protocol name to protocol number */
	if ( (ppe = getprotobyname(protocol.c_str())) == 0)
		cout <<"can't get \""<<protocol.c_str() <<"\" protocol entry\n";
 
    /* Use protocol to choose a socket type */
	if (strcmp(protocol.c_str(), "udp") == 0)
		type = SOCK_DGRAM;
	else
		type = SOCK_STREAM;
        /* Allocate a socket */
	s = socket(PF_INET, type, ppe->p_proto);
	if (s < 0)
		cout <<"can't create socket:" << strerror(errno) << "\n";

    /* Bind the socket */
	if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0)
		cout <<"can't bind to "<< service.c_str() <<" port: "<< strerror(errno) <<"\n";
	if (type == SOCK_STREAM && listen(s, qlen) < 0)
		cout <<"can't listen on "<< service.c_str() <<" port: "<< strerror(errno) <<"\n";
	return s;
}