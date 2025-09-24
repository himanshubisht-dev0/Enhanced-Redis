#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H
#include<string>
#include<vector>
class CommandHandler {
public:
    //split comman into tokens
    static std::vector<std::string> splitArgs(const std::string &input);
    
    //BUild a RESP command from the vector arguments
    static std::string buildRESPcommand(const std::vector<std::string> &args);
private:
};
#endif 