#include "httpreq.hpp"
#include<vector>
#include<sstream>
#include <ranges>
#include<iostream>
using namespace std;


vector<string> split(const string &s, char delim) {
    vector<string> elems;
    for(auto &&e: s|views::split(delim)){
        elems.emplace_back(e.begin(),e.end());
    }
    return elems;
}

string HttpRequest::getHeader(const string& req, const string& key){
    size_t pos = req.find(key);
    if (pos != std::string::npos) {
        pos += key.size();
        size_t end = req.find("\r\n", pos);
        std::string value = req.substr(pos, end - pos);
        return value;
    }
    return "";
}
void HttpRequest::parseQueryParams(const string& query){
    auto pairs = split(query,'&');
    for(auto& p: pairs){
        auto q = split(p,'=');
        if(q.size()==2){
            queryParams[q[0]]=q[1];
        }
    }
}
void HttpRequest::parseHeaders(const string& req){
    if (req.empty()) return;
    auto pos = req.find("\r\n");
    if(pos==string::npos) return;
    string reqLine = req.substr(0,pos);
    vector<string> parts = split(reqLine,' ');
   
    if(parts.size()<2) return;

    if(parts[0]=="GET"){
        method=HttpMethod::GET;
    }
    else if(parts[0]=="POST"){
        method=HttpMethod::POST;
    }
    else if(parts[0]=="PUT"){
        method=HttpMethod::PUT;
    }
    else if(parts[0]=="DELETE"){
        method=HttpMethod::DEL;
    }
    else{
        method = HttpMethod::POST;
    }
    string fullpath = parts[1];

    auto qpos = fullpath.find('?');
    if(qpos!=string::npos){
        path = fullpath.substr(0,qpos);
        query = fullpath.substr(qpos+1);
        parseQueryParams(query);
    }
    else{
        path = fullpath;
        query = "";
    }

    string cl = getHeader(req,"Content-Length: ");
    if(cl.empty()){
        contentLength = 0;
    }
    else{
        contentLength = (size_t)std::atoi(cl.c_str());
    }

    contentType = getHeader(req,"Content-Type: ");

}