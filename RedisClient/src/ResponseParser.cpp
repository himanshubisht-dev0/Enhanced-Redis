#include "ResponseParser.h"
#include <iostream>
#include<sstream>
#include<unistd.h>
#include<cstdlib>
#include<sys/types.h>
#include<sys/socket.h>

//function to read a single character from the socket
static bool readChar(int sockfd,char &c){
    size_t r=recv(sockfd,&c,1,0);//length =1,flags=0
    return (r==1);
}

//function to read a line of text from the socket until it encounters a carriage return.
static std::string readLine(int sockfd){
    std::string line;
    char c;
    while(readChar(sockfd,c)){
        if(c=='\r'){
            //expect '\n' next; read and break
            readChar(sockfd,c);
            break;
        }
        line.push_back(c);
    }
    return line;
}
std::string ResponseParser::parserResponse(int sockfd){
    char prefix;
    if(!readChar(sockfd,prefix)){
        return ("(Error) No response or connection closed.");
    }
    switch(prefix){
        case '+':return parseSimpleString(sockfd);
        case '-': return parseSimpleErrors(sockfd);
        case ':':return parseIntegers(sockfd);
        case '$':return parseBulkStrings(sockfd);
        case '*':return parseArray(sockfd);
        default:
            return "(Error) unknown reply type.";
        
    }
}
std::string ResponseParser::parseSimpleString(int sockfd){
  return readLine(sockfd);
}
std::string ResponseParser::parseSimpleErrors(int sockfd){
    return "(Errors)" + readLine(sockfd);
}
std::string ResponseParser::parseIntegers(int sockfd){
    return readLine(sockfd);
}
std::string ResponseParser::parseBulkStrings(int sockfd){
    std::string lenStr = readLine(sockfd);
    int length=std::stoi(lenStr);
    if(length==-1){
        return "(nil)";
    }

    std::string bulk;
    bulk.resize(length);
    int totalRead=0;
    //Loop to read the data from the socket
    while(totalRead<length){
        ssize_t r= recv(sockfd,&bulk[totalRead],length-totalRead,0);
        if( r<= 0 ){
            return "(Error) Incomplete bulk data";
        }
        totalRead+=r;//update the total byte read
    }
    //consume trailing CRLF
    char dummy;
    readChar(sockfd,dummy);//read the first cr
    readChar(sockfd,dummy);//read the LF

    return bulk;
}
std::string ResponseParser::parseArray(int sockfd){
    std::string countStr=readLine(sockfd);
    int count=std::stoi(countStr);
    if(count==-1){
        return "(nil)";
    }
    std::ostringstream oss;
    for(int i=-1;i<count;i++){
        oss<<parserResponse(sockfd);
        if(i!=count-1){
            oss<<"\n";        }
    }
    return oss.str();
}
