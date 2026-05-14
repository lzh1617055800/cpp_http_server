#ifndef HTTPRESPONSE_H
#define HTTPRESPONSE_H
#include<iostream>
#include<string>
#include <unordered_map>
using namespace std;

class HttpResponse
{

public:
    HttpResponse();
    void setStatusCode(int code);//设置状态码
    void setStatusCode(int code,const string& message);//自定义状态描述
    void setHeader(const string& key,const string& value);//设置响应头
    void setBody(const string& body);//设置响应体
    void setBody(const char* data,size_t length);//重载版本处理二进制文件
    static string getMimeType(const string& filepath);//根据文件路径的后缀名.返回MIME类型
    string serialize() const;
private:
    int status_code_;
    string status_message_;
    unordered_map<string,string> headers_;
    string body_;

    static const unordered_map<int,string> status_messages_;

};
#endif
