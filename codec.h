/////////////////////////////////
//  protobuf message format:
//
//  +----------------------+
//  +   head_length        +  32bit
//  +----------------------+
//  +   type_name_length   +  32bit
//  +----------------------+
//  +   type_name + '\0'   +   
//  +----------------------+
//  +   message            +
//  +----------------------+
//
/////////////////////////////////

#ifndef PAXOSLEASE_CODEC_H_
#define PAXOSLEASE_CODEC_H_

#include <arpa/inet.h> //htonl, ntohl
#include <string>
#include <algorithm>

#include "message.pb.h"

const int kHeadLengthSpace = sizeof(int32_t);
const int kTypeNameLengthSpace = sizeof(int32_t);

inline std::string Encode(const google::protobuf::Message& message)
{
    std::string result;

    result.resize(kHeadLengthSpace);

    const std::string& type_name = message.GetTypeName();
    int32_t type_name_length = static_cast<int32_t>(type_name.size() + 1);
    int32_t be32 = ::htonl(type_name_length);
    result.append(reinterpret_cast<char*>(&be32), sizeof(be32));
    result.append(type_name.c_str(), type_name_length);
    
    bool succeed = message.AppendToString(&result);
    if(succeed) 
    {
        int32_t head_length = ::htonl(result.size() - kHeadLengthSpace);
        std::copy(reinterpret_cast<char*>(&head_length), reinterpret_cast<char*>(&head_length) + sizeof(head_length), result.begin());
        return result;
    } 
    else 
    {
        result.clear();
    }
    return result;
}

inline int32_t BufToInt32(const char* buf)
{
    int32_t be32 = 0;
    ::memcpy(&be32, buf, sizeof(be32));
    return ::ntohl(be32);
}

inline google::protobuf::Message* Name2ProtobufMessage(const std::string& type_name)
{  
    google::protobuf::Message* message = NULL;
    const google::protobuf::Descriptor* descriptor =
        google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(type_name);
    if(descriptor) 
    {
        const google::protobuf::Message* prototype = 
            google::protobuf::MessageFactory::generated_factory()->GetPrototype(descriptor);
        if(prototype) 
        {
            message = prototype->New();
        }
    }
    return message;

}

inline google::protobuf::Message* Decode(const std::string& buf)
{
    google::protobuf::Message* result = NULL;
    
    int32_t length = static_cast<int32_t>(buf.size());
    if(length >= kHeadLengthSpace + kTypeNameLengthSpace)
    {
        int32_t head_length = BufToInt32(buf.c_str());
        
        int32_t type_name_length = BufToInt32(buf.c_str() + kHeadLengthSpace);

        if(length >= type_name_length + kHeadLengthSpace + kTypeNameLengthSpace)
        {
            const std::string type_name(buf.begin() + kHeadLengthSpace + kTypeNameLengthSpace, buf.begin() + kHeadLengthSpace + kTypeNameLengthSpace + type_name_length);
            google::protobuf::Message* message =  Name2ProtobufMessage(type_name);
            
            const char* data = buf.c_str() + kHeadLengthSpace + kTypeNameLengthSpace + type_name_length;
            if(message->ParseFromArray(data, head_length- kTypeNameLengthSpace - type_name_length)) 
            {
                result = message;
            } 
            else
            {
                delete message;
            }
        } 
    }
    return result;
}

#endif //PAXOSLEASE_CODEC_H_
