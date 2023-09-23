#pragma once
#include "utils.h"
#include <jsoncpp/json/json.h>
using namespace std;

class stat_manager
{
    filesystem::recursive_directory_iterator root_;
    unordered_map<filesystem::path, unordered_map<string, time_t>> data_;
    mutex mtx_;
    public:
    void update(const filesystem::path &path)
    {
        // cout<<path<<" updated\n";
        unique_lock<mutex> lk(mtx_);
        struct stat st;
        stat(path.c_str(), &st);
        data_[path]["mtime"] = st.st_mtim.tv_sec;
        data_[path]["size"] = st.st_size;
        data_[path]["atime"]=st.st_atim.tv_sec;
        data_[path]["count"] = 0;
        data_[path]["zipped"]=0;
    }


    stat_manager(const string &path) : root_(path)
    {
        filesystem::recursive_directory_iterator end;
        for (auto iter{root_}; iter != end; ++iter)
        {
            unordered_map<string, uint64_t> v;
            update(iter->path());
        }
    }
    void use(const filesystem::path &path)
    {
        unique_lock<mutex> lk(mtx_);
        lk.unlock();
        data_[path]["atime"]=time(nullptr);
        (data_[path])["count"] += 1;
        update_index_html();
    }
    void update_index_html();
    void zipfile(const filesystem::path &path);
    void print(){
        Json::Value root;
        Json::StyledWriter r;
        for(auto &[k,v]:data_){
            Json::Value temp;
            for(auto&[k1,v1]:v){
                temp[k1]=v1;
            }
            root[k]=temp;
        }
        cout<<r.write(root);
        
    }
};