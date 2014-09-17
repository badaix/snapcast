#include "streamServer.h"



StreamSession::StreamSession(std::shared_ptr<tcp::socket> _socket) : ServerConnection(NULL, _socket)
{
}


void StreamSession::worker()
{
    active_ = true;
    try
    {
        boost::asio::streambuf streambuf;
        std::ostream stream(&streambuf);
        for (;;)
        {
            shared_ptr<BaseMessage> message(messages.pop());
            ServerConnection::send(message.get());
        }
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception in thread: " << e.what() << "\n";
        active_ = false;
    }
    active_ = false;
}


void StreamSession::send(shared_ptr<BaseMessage> message)
{
    if (!message)
        return;

    while (messages.size() > 100)// chunk->getDuration() > 10000)
        messages.pop();
    messages.push(message);
}





StreamServer::StreamServer(unsigned short port) : port_(port), headerChunk(NULL)
{
}


void StreamServer::acceptor()
{
    tcp::acceptor a(io_service_, tcp::endpoint(tcp::v4(), port_));
    for (;;)
    {
        socket_ptr sock(new tcp::socket(io_service_));
        struct timeval tv;
        tv.tv_sec  = 5;
        tv.tv_usec = 0;
        setsockopt(sock->native(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(sock->native(), SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        a.accept(*sock);
        cout << "StreamServer::New connection: " << sock->remote_endpoint().address().to_string() << "\n";
        StreamSession* session = new StreamSession(sock);
        sessions.insert(shared_ptr<StreamSession>(session));
        session->start();
    }
}


void StreamServer::send(shared_ptr<BaseMessage> message)
{
    for (std::set<shared_ptr<StreamSession>>::iterator it = sessions.begin(); it != sessions.end(); )
    {
        if (!(*it)->active())
        {
            cout << "Session inactive. Removing\n";
            sessions.erase(it++);
        }
        else
            ++it;
    }

    for (auto s : sessions)
        s->send(message);
}


void StreamServer::start()
{
    acceptThread = new thread(&StreamServer::acceptor, this);
}


void StreamServer::stop()
{
//		acceptThread->join();
}



