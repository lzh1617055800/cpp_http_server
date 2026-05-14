#include "HttpRequest.h"
#include "Logger.h"
#include <sstream>
#include <algorithm>
#include <iostream>
using namespace std;

bool HttpRequest::parse(const string& data)
{
    istringstream iss(data);
    string line;

    if(!getline(iss,line))return false;
    if(!line.empty() && line.back() == '\r')line.pop_back();
    if(!parseRequsetLine(line)) return false;

    while(getline(iss,line))
    {
        if(!line.empty() && line.back() == '\r')line.pop_back();
        if(line.empty())break;
        if(!parseHeader(line))return false;
    }

    size_t body_start = data.find("\r\n\r\n");
    if(body_start != string::npos)
    {
        //找到了请求体位置
        body = data.substr(body_start + 4);
        LOG_INFO("请求体内容: " + body);
    }
    return true;
}

bool HttpRequest::parseRequsetLine(const string& line)
{
    istringstream iss(line);
    string method_str,version_str;
    if(!(iss>>method_str>>path>>version_str))return false;

    transform(method_str.begin(),method_str.end(),method_str.begin(), ::toupper);
    if(method_str == "GET")method =Method::GET;
    else if(method_str == "POST")method = Method::POST;
    else method =Method::UNKNOWN;

    if(version_str == "HTTP/1.1")version =Version::HTTP_11;
    else if(version_str == "HTTP/1.0")version = Version::HTTP_10;
    else version = Version::UNKNOWN;

    return true;
}

bool HttpRequest::parseHeader(const string& line)
{
    size_t colon_Pos = line.find(':');
    if(colon_Pos == string::npos)return false;

    string key = line.substr(0,colon_Pos);
    string value = line.substr(colon_Pos+1);
    key.erase(0,key.find_first_not_of(" "));
    key.erase(key.find_last_not_of(" ")+1);
    value.erase(0,value.find_first_not_of(" "));

    headers[key] = value;
    return true;
}

const string& HttpRequest::getHeader(const string& key)const
{
    static string empty_str;
    auto it = headers.find(key);
    if(it != headers.end())return it->second;
    return empty_str;
}