#ifndef PAXOSLEASE_DISPATHER_H
#define PAXOSLEASE_DISPATHER_H

#include <map>
#include <memory>
#include <functional>

#include "message.pb.h"


namespace paxoslease {

class Callback {
public:
    virtual ~Callback() {};
    virtual void OnMessage(google::protobuf::Message* message) const = 0;
};

template <typename T>
class CallbackObj: public Callback
{
public:
    // typedef void (*ProtobufMessageCallback)(T*);
    typedef std::function<void(T* message)> ProtobufMessageCallback;

    CallbackObj(const ProtobufMessageCallback& callback)
        : callback_(callback)
    {
        assert(callback_ != 0);
    }

    virtual void OnMessage(google::protobuf::Message* message) const
    {
        T* t = dynamic_cast<T*>(message);
        assert(t != 0);
        callback_(t);
    }

private:
    ProtobufMessageCallback callback_;
};

class ProtobufDispatcher {
public:
    void OnMessage(google::protobuf::Message* message) const
    {
        CallbackMap::const_iterator it = callbacks_.find(message->GetDescriptor());
        if(it != callbacks_.end())
        {
            it->second->OnMessage(message);
        }
    }

    template <typename T>
    void RegisterMessageCallback(const typename CallbackObj<T>::ProtobufMessageCallback& callback)
    {
        std::shared_ptr<CallbackObj<T> > sp(new CallbackObj<T>(callback));
        callbacks_[T::descriptor()] = sp;
    }

    
private:
    typedef std::map<const google::protobuf::Descriptor*, std::shared_ptr<Callback> > CallbackMap;
    
    CallbackMap callbacks_;
};

} // namespace

#endif //PAXOSLEASE_DISPATHER_H
