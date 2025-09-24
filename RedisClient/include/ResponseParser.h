#ifndef RESPONSEPARSER_H

#define RESPONSEPARSER_H
#include<string>
class ResponseParser{
public:
    //Read from the given socket and return parsed response a string, return "" in failure
    static std::string parserResponse(int sockfd);

private:
//Redis Serialisation protocol 
    static std::string parseSimpleString(int sockfd);
    static std::string parseSimpleErrors(int sockfd);
    static std::string parseIntegers(int sockfd);
    static std::string parseBulkStrings(int sockfd);
    static std::string parseArray(int sockfd);


};
#endif //RESPONSEPARSER_H