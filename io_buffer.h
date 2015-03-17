#ifndef PAXOSLEASE_IO_BUFFER_H
#define PAXOSLEASE_IO_BUFFER_H

#include <list>
#include <memory>
#include <stddef.h>

namespace paxoslease {

class IOBufferAllocator {
protected:
    IOBufferAllocator() {}
    virtual ~IOBufferAllocator() {}
public:
    virtual size_t GetBufferSize() const = 0;
    virtual char*  Allocate()            = 0;
    virtual void   Deallocate(char* buf) = 0;
};

bool SetIOBufferAllocator(IOBufferAllocator* allocator);

class IOBufferData {
public:
    IOBufferData();
    IOBufferData(int buf_size);
    IOBufferData(char* buf, int offset, int size, IOBufferAllocator& allocator);
    IOBufferData(char* buf, int buf_size, int offset, int size);

    /// Create an IOBufferData blob by sharing data block from other, set the 
    /// producer_/consumer_ based on the start/end positions that are passed in.
    IOBufferData(const IOBufferData &other, char* s, char* e, char* p = 0);
    ~IOBufferData();

    int Read(int fd, int max_read_ahead);

    int Write(int fd);

    /// Copy data into the buffer. For doing a copy, data is appended to the
    /// buffer staring at the offset corresponding to consumer_. # of bytes
    /// copyied is min(# of bytes, space avail), where space avial = end_ -
    /// producer_. As a result of copy, the producer_ pointer is advanced.
    ///
    int CopyIn(const char* buf, int num_bytes);

    /// share data block from other.
    int Copy(const IOBufferData* other, int num_bytes);

    /// Copy data out of the buffer. For doing a copy, data is copied out of the
    /// buffer staring at the offset corresponding to consumer_. # of bytes
    /// copyied is min(# of bytes, bytes avail), where bytes avial = producer_ -
    /// consumer_.
    ///
    /// Note: As a result of copy, the "consumer_" pointer is not advanced.
    int CopyOut(char* buf, int num_bytes) const;

    inline char* Producer() { return producer_; }
    inline char* Consumer() { return consumer_; }
    inline const char* Producer() const { return producer_; }
    inline const char* Consumer() const { return consumer_; }

    // some data has been filled in the buffer. So advance producer_
    int Fill(int num_bytes);
    int ZeroFill(int num_bytes);

    // advance consumer_
    int Consume(int num_bytes);

    // pull back producer_
    int Trim(int num_bytes);

    size_t BytesConsumable() const { return producer_ - consumer_; }
    size_t SpaceAvailable()  const { return end_ - producer_;}

    bool IsFull() const { return producer_ >= end_; }
    bool IsEmpty() const { return producer_ <= consumer_; }

    bool IsShared() const {
        return (!data_share_ptr_.unique());
    }

    static int default_buffer_size() { return default_buffer_size_; }

private:
    typedef std::shared_ptr<char> IOBufferBlockPtr;

    IOBufferBlockPtr data_share_ptr_;
    char* end_;
    char* producer_;
    char* consumer_;

    inline void Init(char* buf, int buf_size);
    inline void Init(char* buf, IOBufferAllocator& allocator);

    inline int MaxAvailable(int num_bytes) const;
    inline int MaxConsumable(int num_bytes) const;

    static int default_buffer_size_;

};

/// An IOBuffer consists of a list of IOBufferData. Operations on IOBuffer
/// transfers to operations on appropriate IOBufferData.

class IOBuffer {
private:
    typedef std::list<IOBufferData> BList;
public:
    IOBuffer();
    ~IOBuffer();

    /// Clone the contents of an IOBuffer by block sharing.
    IOBuffer *Clone() const;
    
    void Append(const IOBufferData& buf);
    
    int Append(IOBuffer* io_buf);

    /// move data from one buffer to another, This involves (mostly) shuffing pointers
    /// without incurring data copying.
    int Move(IOBuffer* other, int num_bytes);

    void Move(IOBuffer* other);
 
    /// Zero fill the buffer for length num_bytes.
    void ZeroFill(int num_bytes);

    /// Copy data into the buffer. For doing a copy, data is append to the last
    /// IOBufferData in my buf_list_. If the amount of data to be copied exceeds
    /// Space in the last IOBufferData, additonal buffers are allocated and copy
    /// operations runs to finish. As a result of copy, the "producer_" pointer
    /// of an IOBufferData is advanced.
    ///
    int CopyIn(const char* buf, int num_bytes);

    /// Copy data from other. For doing a copy, additonal IOBufferData are allocated and   
    /// appended to buf_list_. It is different from CopyIn that this copy share data block 
    /// from other.
    int Copy(const IOBuffer* other, int num_bytes);

    /// Copy data out of the buffer, For doing a copy, data is copied from the
    /// first iobufferdata in buf_list_, If the amount of data to be copied
    /// exceeds what is available in the first iobufferdata, the list of buffers
    /// is walked to copy out of data.
    ///
    /// NOTE: As a result of copy, the "consumer_" pointer of an IOBufferData is not
    /// advanced.
    int CopyOut(char* buf, int num_bytes) const;
    
    /// Consuming the data in the IOBuffer translates to advancing the "consumer_" 
    /// pointer on underlying IOBufferData. From the first of the list, the "consumer_"
    /// pointer will be advanced on sufficient # of buffers.
    int Consume(int num_bytes);

    /// Trim the data from the end of the buffer to nbytes. This is the converse
    /// of consume, where data is removed from the front of the buffer.
    int Trim(int num_bytes);

    inline int BytesConsumable() const { return byte_count_; }

    inline bool IsEmpty() const { return byte_count_ <= 0; }

    int Read(int fd, int max_read_ahead = -1);

    int Write(int fd);

    void Clear() {
        buf_list_.clear();
        byte_count_ = 0;
    }

private:
    BList buf_list_;
    int byte_count_;

    IOBuffer(const IOBuffer&);
    IOBuffer& operator =(const IOBuffer&);
};

} // namespace paxoslease

#endif //PAXOSLEASE_IO_BUF_H
