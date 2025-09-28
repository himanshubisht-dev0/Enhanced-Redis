#ifndef REDIS_DATABASE_H
#define REDIS_DATABASE_H

#include<string>
#include<mutex>//thread safety if needed in future
#include<unordered_map>
#include<vector>
class RedisDatabase {
public:
    //Get the singleton instance 
    static RedisDatabase& getInstance();
    //Persistent: Dump /load the database from a file.
    bool dump(const std::string& filename);
    bool load(const std::string& filename);
private:
    RedisDatabase()=default;
    ~RedisDatabase()=default;
    RedisDatabase(const RedisDatabase&)=delete;
    RedisDatabase& operator=(const RedisDatabase&) =delete;
    
    std::mutex db_mutex;
    std::unordered_map<std::string,std::string> kv_store;
    std::unordered_map<std::string,std::vector<std::string>> list_store;
    std::unordered_map<std::string,std::unordered_map<std::string,std::string>> hash_store;//hash of key-value pairs


    
};

#endif