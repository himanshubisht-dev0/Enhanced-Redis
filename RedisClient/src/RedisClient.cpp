/*
 Establish TCP connection to redis(RedisClient)
    Berkeley sockets to open TCP connection
    IPv4 and IPv6 'getaddrinfo'
    Implements:
        connectToServer()->Establishes the connection.
        sendCommand()->Sends a command over the socket.
        disconnect()->closes the socket when finished.
*/
#include "RedisClient.h"

RedisClient::RedisClient(const std::string &host,int port)
    :host(host),port(port),sockfd(-1){}
RedisClient::~RedisClient(){
    disconnect();
}

bool RedisClient::connectToServer(){
    struct addrinfo hints,*res =nullptr;
    std::memset(&hints,0,sizeof(hints));//for saftey reasons clear hints section
    hints.ai_family=AF_UNSPEC;//IPv4 or IPv6 can be anything
    hints.ai_socktype=SOCK_STREAM;// use TCP
    
    std::string portstr=std::to_string(port);//convert int to string
    int err=getaddrinfo(host.c_str(),portstr.c_str(),&hints,&res);//resolving the host and port to a linkedlist of address infrastructure 
    if(err!=0){
       std::cerr<<"getaddrinfo:"<<gai_strerror(err)<<"\n";//print error message
       return false;//failed to resolve
    }
    //try to connect to one of the resolved addresses
    for(auto p=res;p!=nullptr;p=p->ai_next){
        sockfd=socket(p->ai_family,p->ai_socktype,p->ai_protocol);//create socket
        if(sockfd==-1)continue;//failed to create socket try next
        if(connect(sockfd,p->ai_addr,p->ai_addrlen)==0)break;// break on successfully connected
        close(sockfd);//failed to connect close socket and try next
        sockfd=-1; //reset socket file descriptor
    }
    freeaddrinfo(res);//free the address info
    if(sockfd==-1){
        std::cerr<<"could not connect to"<<host<<":"<<port<<"\n";//print failure message
        return false;
    }
    return true;//return true on successful connection
}

void RedisClient::disconnect(){
    if(sockfd!=-1){
        close(sockfd);//close the socket if connection failed
        sockfd=-1;//reset socket file descriptor
    }
}
int RedisClient::getSocketFD() const{
    return sockfd;
}
bool RedisClient::sendCommand(const std::string &command){
    if(sockfd==-1)return false;
    size_t sent=send(sockfd,command.c_str(),command.size(),0);
    return (sent==(size_t)command.size());
}