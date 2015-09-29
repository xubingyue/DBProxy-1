#include <assert.h>
#include "Client.h"

#include "SSDBProtocol.h"
#include "SSDBWaitReply.h"

static const std::string SSDB_OK = "ok";
static const std::string SSDB_ERROR = "error";

static void syncSSDBStrList(ClientLogicSession* client, const std::vector<std::string>& strList)
{
    static SSDBProtocolRequest strsResponse;
    strsResponse.init();

    strsResponse.writev(strList);
    strsResponse.endl();

    client->send(strsResponse.getResult(), strsResponse.getResultLen());
}

StrListSSDBReply::StrListSSDBReply(ClientLogicSession* client) : BaseWaitReply(client)
{
}

void StrListSSDBReply::onBackendReply(int64_t dbServerSocketID, const char* buffer, int len)
{

}

void StrListSSDBReply::mergeAndSend(ClientLogicSession* client)
{
    mStrListResponse.endl();
    client->send(mStrListResponse.getResult(), mStrListResponse.getResultLen());
}

void StrListSSDBReply::pushStr(std::string&& str)
{
    mStrListResponse.writev(str);
}

void StrListSSDBReply::pushStr(const std::string& str)
{
    mStrListResponse.writev(str);
}

void StrListSSDBReply::pushStr(const char* str)
{
    mStrListResponse.appendStr(str);
}

SSDBSingleWaitReply::SSDBSingleWaitReply(ClientLogicSession* client) : BaseWaitReply(client)
{
}

/*  TODO::�������ظ����ǵ�һ��pending reply����ô���Բ��û����ֱ�ӷ��͸��ͻ���(�����ڴ濽��)  */
void SSDBSingleWaitReply::onBackendReply(int64_t dbServerSocketID, const char* buffer, int len)
{
    for (auto& v : mWaitResponses)
    {
        if (v.dbServerSocketID == dbServerSocketID)
        {
            v.reply = new std::string(buffer, len);
            break;
        }
    }
}

void SSDBSingleWaitReply::mergeAndSend(ClientLogicSession* client)
{
    if (mErrorCode != nullptr)
    {
        syncSSDBStrList(client, { SSDB_ERROR, *mErrorCode });
    }
    else
    {
        client->send(mWaitResponses.front().reply->c_str(), mWaitResponses.front().reply->size());
    }
}

SSDBMultiSetWaitReply::SSDBMultiSetWaitReply(ClientLogicSession* client) : BaseWaitReply(client)
{
}

void SSDBMultiSetWaitReply::onBackendReply(int64_t dbServerSocketID, const char* buffer, int len)
{
    for (auto& v : mWaitResponses)
    {
        if (v.dbServerSocketID == dbServerSocketID)
        {
            v.reply = new std::string(buffer, len);

            if (mWaitResponses.size() != 1)
            {
                v.ssdbReply = new SSDBProtocolResponse;
                v.ssdbReply->parse(v.reply->c_str());
            }

            break;
        }
    }
}

void SSDBMultiSetWaitReply::mergeAndSend(ClientLogicSession* client)
{
    if (mErrorCode != nullptr)
    {
        syncSSDBStrList(client, { SSDB_ERROR, *mErrorCode });
    }
    else
    {
        if (mWaitResponses.size() == 1)
        {
            client->send(mWaitResponses.front().reply->c_str(), mWaitResponses.front().reply->size());
        }
        else
        {
            string* errorReply = nullptr;
            int64_t num = 0;

            for (auto& v : mWaitResponses)
            {
                int64_t tmp;
                if (read_int64(v.ssdbReply, &tmp).ok())
                {
                    num += tmp;
                }
                else
                {
                    errorReply = v.reply;
                    break;
                }
            }

            if (errorReply != nullptr)
            {
                client->send(errorReply->c_str(), errorReply->size());
            }
            else
            {
                static SSDBProtocolRequest response;
                response.init();

                response.writev(SSDB_OK, num);
                response.endl();
                client->send(response.getResult(), response.getResultLen());
            }
        }
    }
}

SSDBMultiGetWaitReply::SSDBMultiGetWaitReply(ClientLogicSession* client) : BaseWaitReply(client)
{
}

void SSDBMultiGetWaitReply::onBackendReply(int64_t dbServerSocketID, const char* buffer, int len)
{
    for (auto& v : mWaitResponses)
    {
        if (v.dbServerSocketID == dbServerSocketID)
        {
            v.reply = new std::string(buffer, len);

            if (mWaitResponses.size() != 1)
            {
                v.ssdbReply = new SSDBProtocolResponse;
                v.ssdbReply->parse(v.reply->c_str());
            }

            break;
        }
    }
}

void SSDBMultiGetWaitReply::mergeAndSend(ClientLogicSession* client)
{
    if (mErrorCode != nullptr)
    {
        syncSSDBStrList(client, { SSDB_ERROR, *mErrorCode });
    }
    else
    {
        if (mWaitResponses.size() == 1)
        {
            client->send(mWaitResponses.front().reply->c_str(), mWaitResponses.front().reply->size());
        }
        else
        {
            string* errorReply = nullptr;

            static vector<Bytes> kvs;
            kvs.clear();

            for (auto& v : mWaitResponses)
            {
                if (!read_bytes(v.ssdbReply, &kvs).ok())
                {
                    errorReply = v.reply;
                    break;
                }
            }

            if (errorReply != nullptr)
            {
                client->send(errorReply->c_str(), errorReply->size());
            }
            else
            {
                static SSDBProtocolRequest strsResponse;
                strsResponse.init();

                strsResponse.writev(SSDB_OK);
                for (auto& v : kvs)
                {
                    strsResponse.appendStr(v.buffer, v.len);
                }
                
                strsResponse.endl();
                client->send(strsResponse.getResult(), strsResponse.getResultLen());
            }
        }
    }
}