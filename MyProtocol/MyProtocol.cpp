//
//
//		手把手教你实现自定义的应用层协议
//		http://www.acb0y.cn/?p=457
//
//
#include <stdint.h>
#include <stdio.h>
#include <queue>
#include <vector>
#include <iostream>
#include <string.h>
#include "jsoncpp/json.h"
#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib,"Ws2_32.lib")
typedef int socklen_t ;
#else
#include <arpa/inet.h>
#endif

using namespace std;
const uint8_t MY_PROTO_MAGIC = 88;
const uint32_t MY_PROTO_MAX_SIZE = 10 * 1024 * 1024; //10M
const uint32_t MY_PROTO_HEAD_SIZE = 8;
typedef enum MyProtoParserStatus
{
    ON_PARSER_INIT = 0,
    ON_PARSER_HAED = 1,
    ON_PARSER_BODY = 2,
}MyProtoParserStatus;

/*
    协议头
 */
struct MyProtoHead
{
    uint8_t version;    //协议版本号
    uint8_t magic;      //协议魔数
    uint16_t server;    //协议复用的服务号，标识协议之上的不同服务
    uint32_t len;       //协议长度（协议头长度+变长json协议体长度）
};

/*
    协议消息体
 */
struct MyProtoMsg
{
    MyProtoHead head;   //协议头
    Json::Value body;   //协议体
};

void myProtoMsgPrint(MyProtoMsg & msg)
{
    string jsonStr = "";
    Json::FastWriter fWriter;
    jsonStr = fWriter.write(msg.body);
    printf("Head[version=%d,magic=%d,server=%d,len=%d],Body:%s",
        msg.head.version, msg.head.magic, 
        msg.head.server, msg.head.len, jsonStr.c_str());
}

/*
    MyProto打包类
 */
class MyProtoEnCode
{
public:
    //协议消息体打包函数
    uint8_t * encode(MyProtoMsg * pMsg, uint32_t & len);
private:
    //协议头打包函数
    void headEncode(uint8_t * pData, MyProtoMsg * pMsg);
};

void MyProtoEnCode::headEncode(uint8_t * pData, MyProtoMsg * pMsg)
{
    //设置协议头版本号为1
    *pData = 1; 
    ++pData;
    //设置协议头魔数
    *pData = MY_PROTO_MAGIC;
    ++pData;
    //设置协议服务号，把head.server本地字节序转换为网络字节序
    *(uint16_t *)pData = htons(pMsg->head.server);
    pData += 2;
    //设置协议总长度，把head.len本地字节序转换为网络字节序
    *(uint32_t *)pData = htonl(pMsg->head.len);
}

uint8_t * MyProtoEnCode::encode(MyProtoMsg * pMsg, uint32_t & len)
{
    uint8_t * pData = NULL;
    Json::FastWriter fWriter;
    //协议json体序列化
    string bodyStr = fWriter.write(pMsg->body);
    //计算协议消息序列化后的总长度
    len = MY_PROTO_HEAD_SIZE + (uint32_t)bodyStr.size();
    pMsg->head.len = len;
    //申请协议消息序列化需要的空间
    pData = new uint8_t[len];
    //打包协议头
    headEncode(pData, pMsg);
    //打包协议体
    memcpy(pData + MY_PROTO_HEAD_SIZE, bodyStr.data(), bodyStr.size());
    return pData;
}

/*
    MyProto解包类
 */
class MyProtoDeCode
{
public:
    void init();
    void clear();
    bool parser(void * data, size_t len);
    bool empty();
    MyProtoMsg * front();
    void pop();
private:
    bool parserHead(uint8_t ** curData, uint32_t & curLen, 
        uint32_t & parserLen, bool & parserBreak);
    bool parserBody(uint8_t ** curData, uint32_t & curLen, 
        uint32_t & parserLen, bool & parserBreak);
private:
    MyProtoMsg mCurMsg;                     //当前解析中的协议消息体
    queue<MyProtoMsg *> mMsgQ;              //解析好的协议消息队列
    vector<uint8_t> mCurReserved;           //未解析的网络字节流
    MyProtoParserStatus mCurParserStatus;   //当前解析状态
};

void MyProtoDeCode::init()
{
    mCurParserStatus = ON_PARSER_INIT;
}

void MyProtoDeCode::clear()
{
    MyProtoMsg * pMsg = NULL;
    while (!mMsgQ.empty())
    {
        pMsg = mMsgQ.front();
        delete pMsg;
        mMsgQ.pop();
    }
}

bool MyProtoDeCode::parserHead(uint8_t ** curData, uint32_t & curLen, 
    uint32_t & parserLen, bool & parserBreak)
{
    parserBreak = false;
    if (curLen < MY_PROTO_HEAD_SIZE)
    {
        parserBreak = true; //终止解析
        return true;
    }
    uint8_t * pData = *curData;
    //解析版本号
    mCurMsg.head.version = *pData;
    pData++;
    //解析魔数
    mCurMsg.head.magic = *pData;
    pData++;
    //魔数不一致，则返回解析失败
    if (MY_PROTO_MAGIC != mCurMsg.head.magic)
    {
        return false;
    }
    //解析服务号
    mCurMsg.head.server = ntohs(*(uint16_t*)pData);
    pData+=2;
    //解析协议消息体的长度
    mCurMsg.head.len = ntohl(*(uint32_t*)pData);
    //异常大包，则返回解析失败
    if (mCurMsg.head.len > MY_PROTO_MAX_SIZE)
    {
        return false;
    }
    //解析指针向前移动MY_PROTO_HEAD_SIZE字节
    (*curData) += MY_PROTO_HEAD_SIZE;
    curLen -= MY_PROTO_HEAD_SIZE;
    parserLen += MY_PROTO_HEAD_SIZE;
    mCurParserStatus = ON_PARSER_HAED;
    return true;
}

bool MyProtoDeCode::parserBody(uint8_t ** curData, uint32_t & curLen, 
    uint32_t & parserLen, bool & parserBreak)
{
    parserBreak = false;
    uint32_t jsonSize = mCurMsg.head.len - MY_PROTO_HEAD_SIZE;
    if (curLen < jsonSize)
    {
        parserBreak = true; //终止解析
        return true;
    }
    Json::Reader reader;    //json解析类
    if (!reader.parse((char *)(*curData), 
        (char *)((*curData) + jsonSize), mCurMsg.body, false))
    {
        return false;
    }
    //解析指针向前移动jsonSize字节
    (*curData) += jsonSize;
    curLen -= jsonSize;
    parserLen += jsonSize;
    mCurParserStatus = ON_PARSER_BODY;
    return true;
}

bool MyProtoDeCode::parser(void * data, size_t len)
{
    if (len <= 0)
    {
        return false;
    }
    uint32_t curLen = 0;
    uint32_t parserLen = 0;
    uint8_t * curData = NULL;
    curData = (uint8_t *)data;
    //把当前要解析的网络字节流写入未解析完字节流之后
    while (len--)
    {
        mCurReserved.push_back(*curData);
        ++curData;
    }
    curLen = mCurReserved.size();
    curData = (uint8_t *)&mCurReserved[0];
    //只要还有未解析的网络字节流，就持续解析
    while (curLen > 0)
    {
        bool parserBreak = false;
        //解析协议头
        if (ON_PARSER_INIT == mCurParserStatus ||
            ON_PARSER_BODY == mCurParserStatus)
        {
            if (!parserHead(&curData, curLen, parserLen, parserBreak))
            {
                return false;
            }
            if (parserBreak) break;
        }
        //解析完协议头，解析协议体
        if (ON_PARSER_HAED == mCurParserStatus)
        {
            if (!parserBody(&curData, curLen, parserLen, parserBreak))
            {
                return false;
            }
            if (parserBreak) break;
        }
        if (ON_PARSER_BODY == mCurParserStatus)
        {
            //拷贝解析完的消息体放入队列中
            MyProtoMsg * pMsg = NULL;
            pMsg = new MyProtoMsg;
            *pMsg = mCurMsg;
            mMsgQ.push(pMsg);
        }
    }
    if (parserLen > 0)
    {
        //删除已经被解析的网络字节流
        mCurReserved.erase(mCurReserved.begin(), mCurReserved.begin() + parserLen);
    }
    return true;
}

bool MyProtoDeCode::empty()
{
    return mMsgQ.empty();
}

MyProtoMsg * MyProtoDeCode::front()
{
    MyProtoMsg * pMsg = NULL;
    pMsg = mMsgQ.front();
    return pMsg;
}

void MyProtoDeCode::pop()
{
    mMsgQ.pop();
}

int main()
{
    uint32_t len = 0;
    uint8_t * pData = NULL;
    MyProtoMsg msg1;
    MyProtoMsg msg2;
    MyProtoDeCode myDecode;
    MyProtoEnCode myEncode;
    msg1.head.server = 1;
    msg1.body["op"] = "set";
    msg1.body["key"] = "id";
    msg1.body["value"] = "9856";
    msg2.head.server = 2;
    msg2.body["op"] = "get";
    msg2.body["key"] = "id";
    myDecode.init();
    pData = myEncode.encode(&msg1, len);
    if (!myDecode.parser(pData, len))
    {
        cout << "parser falied!" << endl;
    }
    else
    {
        cout << "msg1 parser successful!" << endl;
    }
    pData = myEncode.encode(&msg2, len);
    if (!myDecode.parser(pData, len))
    {
        cout << "parser falied!" << endl;
    }
    else
    {
        cout << "msg2 parser successful!" << endl;
    }
    MyProtoMsg * pMsg = NULL;
    while (!myDecode.empty())
    {
        pMsg = myDecode.front();
        myProtoMsgPrint(*pMsg);
        myDecode.pop();
    }
    return 0;
}