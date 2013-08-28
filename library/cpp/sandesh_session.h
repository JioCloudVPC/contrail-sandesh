/*
 * Copyright (c) 2013 Juniper Networks, Inc. All rights reserved.
 */

//
// sandesh_session.h
//
// Sandesh Session
//

#ifndef __SANDESH_SESSION_H__
#define __SANDESH_SESSION_H__

#include <sandesh/transport/TBufferTransports.h>
#include <tbb/mutex.h>
#include <boost/system/error_code.hpp>
#include <boost/asio.hpp>
#include "base/util.h"
#include "io/tcp_session.h"
#include "sandesh.h"

using contrail::sandesh::transport::TMemoryBuffer;
class SandeshSession;
class Sandesh;

class SandeshWriter {
public:
    static const uint32_t kEncodeBufferSize = 2048;
    static const unsigned int kDefaultSendSize = 16384;

    SandeshWriter(TcpSession *session);
    ~SandeshWriter();
    void SendMsg(Sandesh *sandesh, bool more);
    void SendBuffer(boost::shared_ptr<TMemoryBuffer> sbuffer,
            bool more = false) {
        SendInternal(sbuffer);
    }
    void WriteReady(const boost::system::error_code &ec);
    bool SendReady() {
        tbb::mutex::scoped_lock lock(mutex_);
        return ready_to_send_;
    }
    int WaitMsgQSize() { return wait_msgq_.size(); }

    static const std::string sandesh_open_;
    static const std::string sandesh_open_attr_length_;
    static const std::string sandesh_close_;

protected:
    friend class SandeshSessionTest;

    // Inline routines invoked by SendMsg()
    void SendMsgMore(boost::shared_ptr<TMemoryBuffer>);
    void SendMsgAll(boost::shared_ptr<TMemoryBuffer>);

private:
    friend class SandeshSendMsgUnitTest;

    typedef std::vector<boost::shared_ptr<TMemoryBuffer> > WaitMsgQ;

    TcpSession *session_;

    void SendInternal(boost::shared_ptr<TMemoryBuffer>);
    void ConnectTimerExpired(const boost::system::error_code &error);
    size_t send_buf_offset() { return send_buf_offset_; }
    uint8_t* send_buf() const { return send_buf_; }
    void set_send_buf(uint8_t *buf, size_t len) {
        assert(len && (len < kDefaultSendSize));
        memcpy(send_buf(), buf, len);
        send_buf_offset_ = len;
    }
    void append_send_buf(uint8_t *buf, size_t len) {
        assert(len && ((len + send_buf_offset_) < kDefaultSendSize));
        memcpy(send_buf() + send_buf_offset_, buf, len);
        send_buf_offset_ += len;
    }
    void reset_send_buf() {
        send_buf_offset_ = 0;
    }

    bool ready_to_send_;
    WaitMsgQ wait_msgq_;
    tbb::mutex mutex_;
    // send_buf_ is used to store unsent data
    uint8_t *send_buf_;
    size_t send_buf_offset_;

#define sXML_SANDESH_OPEN_ATTR_LENGTH  "<sandesh length=\""
#define sXML_SANDESH_OPEN              "<sandesh length=\"0000000000\">"
#define sXML_SANDESH_CLOSE             "</sandesh>"

    DISALLOW_COPY_AND_ASSIGN(SandeshWriter);
};

class SandeshReader {
public:
    typedef boost::asio::const_buffer Buffer;

    SandeshReader(TcpSession *session);
    virtual ~SandeshReader();
    virtual void OnRead(Buffer buffer);
    static int ExtractMsgHeader(const std::string& msg, SandeshHeader& header,
            std::string& msg_type, uint32_t& header_offset);
    SandeshSession *session();

private:
    bool MsgLengthKnown() { return msg_length_ != (size_t)-1; }

    size_t msg_length() { return msg_length_; }

    void set_msg_length(size_t length) { msg_length_ = length; }

    void reset_msg_length() { set_msg_length(-1); }

    void SetBuf(const std::string &str);
    void ReplaceBuf(const std::string &str);
    bool LeftOver() const;
    int MatchString(const std::string& match, size_t &m_offset);
    bool ExtractMsgLength(size_t &msg_length, int *result);
    bool ExtractMsg(Buffer buffer, int *result, bool NewBuf);

    std::string buf_;
    size_t offset_;
    size_t msg_length_;
    TcpSession *session_;

    static const int kDefaultRecvSize = SandeshWriter::kDefaultSendSize;

    DISALLOW_COPY_AND_ASSIGN(SandeshReader);
};

class SandeshConnection;

class SandeshSession : public TcpSession {
public:
    typedef boost::function<void(const std::string&, SandeshSession *)> ReceiveMsgCb;
    SandeshSession(TcpServer *client, Socket *socket, int sendq_task_instance,
            int sendq_task_id);
    virtual ~SandeshSession();
    virtual void OnRead(Buffer buffer);
    virtual void WriteReady(const boost::system::error_code &ec) {
        writer_->WriteReady(ec);
    }
    virtual bool EnqueueBuffer(u_int8_t *buf, u_int32_t buf_len);
    Sandesh::SandeshQueue *send_queue() {
        return send_queue_.get();
    }
    Sandesh::SandeshBufferQueue *send_buffer_queue() {
        return send_buffer_queue_.get();
    }
    SandeshWriter* writer() {
        return writer_.get();
    }
    void SetConnection(SandeshConnection *connection) {
        connection_ = connection;
    }
    SandeshConnection *connection() {
        return connection_;
    }
    void SetReceiveMsgCb(ReceiveMsgCb cb) {
        cb_ = cb;
    }
    ReceiveMsgCb receive_msg_cb() {
        return cb_;
    }
    static Sandesh * 
    DecodeCtrlSandesh(const std::string& msg, const SandeshHeader& header,
        const std::string& sandesh_name, const uint32_t& header_offset);

private:
    friend class SandeshSessionTest;

    bool SendMsg(Sandesh *sandesh);
    bool SendBuffer(boost::shared_ptr<TMemoryBuffer> sbuffer);
    bool SessionSendReady();

    boost::scoped_ptr<SandeshWriter> writer_;
    boost::scoped_ptr<SandeshReader> reader_;
    boost::scoped_ptr<Sandesh::SandeshQueue> send_queue_;
    boost::scoped_ptr<Sandesh::SandeshBufferQueue> send_buffer_queue_;
    SandeshConnection *connection_;
    ReceiveMsgCb cb_;
    tbb::mutex smutex_;

    DISALLOW_COPY_AND_ASSIGN(SandeshSession);
};

#endif // __SANDESH_SESSION_H__
