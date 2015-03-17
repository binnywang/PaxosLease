#include <iostream>
#include <functional>
#include <string>

#include "message.pb.h"
#include "dispatcher.h"
#include "codec.h"

using namespace std;

class Proposer {
public:
    void OnPrepareRequest(paxoslease::PrepareRequest* prepare_request)
    {
        cout << "OnPrepareRequest:" << prepare_request->GetTypeName() << endl;
        cout << "ballot_number:" << prepare_request->ballot_number() << endl;
        cout << "node_id:" << prepare_request->node_id() << endl;
    }
};

class Acceptor {
public:
    void OnPrepareResponse(paxoslease::PrepareResponse* prepare_response)
    {
        cout << "OnPrepareResponse:" << prepare_response->GetTypeName() << endl; 
        cout << "ballot_number:" << prepare_response->ballot_number() << endl;
        cout << "node_id:" << prepare_response->node_id() << endl;
    }
};

void OnProposeRequest(paxoslease::ProposeRequest* propose_request)
{
    cout << "OnProposeRequest:" << propose_request->GetTypeName() << endl; 
}

int main()
{
    cout << "test dispatcher" << endl << endl;
    paxoslease::ProtobufDispatcher dispatcher;
    Proposer proposer;
    Acceptor acceptor;

    dispatcher.RegisterMessageCallback<paxoslease::PrepareRequest>(std::bind(&Proposer::OnPrepareRequest, &proposer, std::placeholders::_1));
    dispatcher.RegisterMessageCallback<paxoslease::PrepareResponse>(std::bind(&Acceptor::OnPrepareResponse, &acceptor, std::placeholders::_1));
    dispatcher.RegisterMessageCallback<paxoslease::ProposeRequest>(std::bind(&OnProposeRequest, std::placeholders::_1));
    
    paxoslease::PrepareRequest  p_q;
    paxoslease::PrepareResponse p_r;
    paxoslease::ProposeRequest  q_q;
    paxoslease::ProposeResponse q_r;

    dispatcher.OnMessage(&p_q);
    dispatcher.OnMessage(&p_r);
    dispatcher.OnMessage(&q_q);


    cout <<"test codec" << endl << endl;

    p_q.set_ballot_number(111);
    p_q.set_node_id(1);

    p_r.set_ballot_number(222);
    p_r.set_node_id(1);
    p_r.set_lease_empty(false);

    q_q.set_ballot_number(333);
    q_q.set_node_id(1);
    
    q_r.set_ballot_number(333);
    q_r.set_node_id(1);

    string p_q_msg = Encode(p_q);
    google::protobuf::Message* message = Decode(p_q_msg);
    dispatcher.OnMessage(message);
    
    string p_r_msg = Encode(p_r);
    message = Decode(p_r_msg);
    dispatcher.OnMessage(message);

    string q_q_msg = Encode(p_r);
    message = Decode(q_q_msg);
    dispatcher.OnMessage(message);
   
    string q_r_msg = Encode(q_r);
    message = Decode(q_r_msg);
    dispatcher.OnMessage(message);

    return 0; 
}
