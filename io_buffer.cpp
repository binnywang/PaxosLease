#include "io_buffer.h"

#include <algorithm>
#include <string.h>
#include <limits.h>
#include <sys/uio.h>
#include <unistd.h> 
#include <assert.h>

namespace paxoslease {

int IOBufferData::default_buffer_size_ =  4 << 10;

static IOBufferAllocator*  s_io_buffer_allocator = 0;
static bool s_used_io_buffer_allocator = false;


struct IOBufferArrayDeallocator {
    void operator ()(char* buf) { 
        delete [] buf; 
    }
};

struct IOBufferDeallocator {
    void operator ()(char* buf) {
        s_io_buffer_allocator->Deallocate(buf); 
    }
};

class IOBufferDeallocatorCustom {
public:
    IOBufferDeallocatorCustom(IOBufferAllocator& allocator)
        : allocator_(allocator_)
    { }
    void operator ()(char* buf) {
        allocator_.Deallocate(buf);
    }
private:
    IOBufferAllocator& allocator_;
};

// if you want change the default allocator, call this function
bool SetIOBufferAllocator(IOBufferAllocator* allocator)
{
    if(s_used_io_buffer_allocator ||
            (allocator && int(allocator->GetBufferSize()) <= 0)) {
        return false;
    }
    s_io_buffer_allocator = allocator;
    return true;
}

inline int IOBufferData::MaxAvailable(int num_bytes) const
{
    return std::max(0, std::min(int(SpaceAvailable()), num_bytes));
}

inline int IOBufferData::MaxConsumable(int num_bytes) const
{
    return std::max(0, std::min(int(BytesConsumable()), num_bytes));
}

inline void IOBufferData::Init(char* buf, int buf_size)
{
    const int size = std::max(0, buf_size);
    data_share_ptr_.reset(buf ? buf : new char[size], IOBufferArrayDeallocator());
    producer_ = data_share_ptr_.get();
    end_= producer_ + size;
    consumer_ = producer_;
}


inline void IOBufferData::Init(char* buf, IOBufferAllocator& allocator)
{
    if(&allocator == s_io_buffer_allocator) {
        if(!s_used_io_buffer_allocator) {
            default_buffer_size_ = s_io_buffer_allocator->GetBufferSize();
        }
        s_used_io_buffer_allocator = true;
        data_share_ptr_.reset(buf ? buf : allocator.Allocate(),
                IOBufferDeallocator());
    } else {
        data_share_ptr_.reset(buf ? buf : allocator.Allocate(),
                IOBufferDeallocatorCustom(allocator));
    }

    if(! (producer_ = data_share_ptr_.get())) {
        abort();
    }

    end_ = producer_ + allocator.GetBufferSize();
    consumer_ = producer_;
}

IOBufferData::IOBufferData()
    : data_share_ptr_(),
      end_(0),
      producer_(0),
      consumer_(0)
{
    if(s_io_buffer_allocator) {
        IOBufferData::Init(0, *s_io_buffer_allocator);
    } else {
        IOBufferData::Init(0, default_buffer_size_);
    }
}

IOBufferData::IOBufferData(int buf_size)
    : data_share_ptr_(),
      end_(0),
      producer_(0),
      consumer_(0)
{
    IOBufferData::Init(0, buf_size);
} 

IOBufferData::IOBufferData(char* buf, int offset, int size, IOBufferAllocator& allocator)
    : data_share_ptr_(),
      end_(0),
      producer_(0),
      consumer_(0)
{
    IOBufferData::Init(buf, allocator);
    IOBufferData::Fill(offset + size);
    IOBufferData::Consume(offset);
}

IOBufferData::IOBufferData(char* buf, int buf_size, int offset, int size)
    : data_share_ptr_(),
      end_(0),
      producer_(0),
      consumer_(0)
{
    IOBufferData::Init(buf, buf_size);
    IOBufferData::Fill(offset + size);
    IOBufferData::Consume(offset);
}

IOBufferData::IOBufferData(const IOBufferData &other, char*s, char* e, char* p /* = 0*/)
    : data_share_ptr_(other.data_share_ptr_),
      end_(e),
      producer_(p ? p : e),
      consumer_(s)
{
    assert(data_share_ptr_.get() <= consumer_ && 
            consumer_ <= producer_ && 
            producer_ <= end_ &&
            end_ <= other.end_);
}

IOBufferData::~IOBufferData()
{
}

int IOBufferData::Fill(int num_bytes)
{
    const int nbytes = MaxAvailable(num_bytes);
    producer_ += nbytes;

    return nbytes;
}

int IOBufferData::ZeroFill(int num_bytes)
{
    const int nbytes = MaxAvailable(num_bytes);
    memset(producer_, '\0', nbytes);
    producer_ += nbytes;
    
    return nbytes;
}

int IOBufferData::Consume(int num_bytes)
{
    const int nbytes = MaxConsumable(num_bytes);
    consumer_ += nbytes;
    
    assert(consumer_ <= producer_);
    return nbytes;
}

int IOBufferData::Trim(int num_bytes)
{
    const int nbytes = MaxConsumable(num_bytes); 
    producer_ = consumer_ + nbytes;
    return nbytes;
}

int IOBufferData::Read(int fd, int max_read_ahead)
{
    const int nbytes = MaxAvailable(max_read_ahead);

    if(nbytes > 0) {
        int nread = read(fd, producer_, nbytes);
        if(nread > 0) {
            producer_ += nread;
        }
        return (nread > 0 ? nread : (errno > 0 ? -errno : nread));
    } else {
        return -1;
    }
}

int IOBufferData::Write(int fd)
{
    const int nbytes = BytesConsumable();
    if(nbytes > 0) {
        int nwrote = write(fd, consumer_, nbytes);
        if(nwrote > 0) {
            consumer_ += nwrote;
        }
        return (nwrote > 0 ? nwrote : (errno > 0 ? -errno : nwrote));
    } else {
        return -1;
    }
}

int IOBufferData::CopyIn(const char* buf, int num_bytes)
{
    const int ncopy = MaxAvailable(num_bytes);
    memmove(producer_, buf, ncopy); 
    producer_ += ncopy;
    return ncopy;
}

int IOBufferData::Copy(const IOBufferData* other, int num_bytes)
{
    const int ncopy = MaxAvailable(num_bytes);
    memmove(producer_, other->Consumer(), ncopy);
    producer_ += ncopy;
    return ncopy;
}

int IOBufferData::CopyOut(char* buf, int num_bytes) const
{
    const int ncopy = MaxConsumable(num_bytes);
    memmove(buf, consumer_, ncopy);
    return ncopy;
}

IOBuffer::IOBuffer()
    : buf_list_(),
      byte_count_(0)
{
}

IOBuffer::~IOBuffer()
{
}
    
IOBuffer* IOBuffer::Clone() const 
{
    IOBuffer* const clone = new IOBuffer();
    if(! clone) {
        return NULL;
    }
    BList::const_iterator it;
    for(it = buf_list_.begin(); it != buf_list_.end(); it++) {
        if(! it->IsEmpty()) {
            clone->buf_list_.push_back(IOBufferData(*it,
                        const_cast<char*>(it->Consumer()),
                        const_cast<char*>(it->Producer())));
        }
    }
    assert(byte_count_ >= 0);
    clone->byte_count_ = byte_count_;
    return clone;
}

void IOBuffer::Append(const IOBufferData& buf)
{
   buf_list_.push_back(buf);
   assert(byte_count_ >= 0);

   byte_count_ += buf.BytesConsumable();
}

int IOBuffer::Append(IOBuffer* io_buf)
{
   if(!io_buf) {
       return -1;
   }

    int nbytes = 0;
    BList::iterator it;
    for(it = io_buf->buf_list_.begin(); it != io_buf->buf_list_.end(); ) {
        const int nb = it->BytesConsumable(); 
        if(nb > 0) {
            buf_list_.splice(buf_list_.end(), io_buf->buf_list_, it++);
            nbytes += nb;
        } else {
            it = io_buf->buf_list_.erase(it);
        }
    }
    
    assert(byte_count_ >= 0 && 
        io_buf->byte_count_ == nbytes && io_buf->buf_list_.empty());
    io_buf->byte_count_ = 0;
    byte_count_ += nbytes;

    return nbytes;
}

void IOBuffer::Move(IOBuffer* other)
{
   assert(other && other->byte_count_ >=0 && byte_count_ >> 0); 

   buf_list_.splice(buf_list_.end(), other->buf_list_);
    
   byte_count_ += other->byte_count_;
   other->byte_count_ = 0;
}

int IOBuffer::Move(IOBuffer* other, int num_bytes)
{
    if(! other || num_bytes < 0) {
        return -1;
    }
    
    int nbytes = other->byte_count_;
    if(num_bytes >= nbytes) {
        IOBuffer::Move(other);
        return nbytes;
    }
    int to_moved = num_bytes;
    while(! other->buf_list_.empty() && to_moved > 0) {
        IOBufferData& data = other->buf_list_.front();
        const int nb = data.BytesConsumable();
        if(to_moved > nb) {
            if(nb > 0) {
                buf_list_.splice(buf_list_.end(), other->buf_list_, other->buf_list_.begin());
                to_moved -= nb;
            } else {
                other->buf_list_.pop_front();
            }
        }
        else {
            buf_list_.push_back(IOBufferData(data, data.Consumer(), data.Consumer() + to_moved));
            to_moved -= data.Consume(to_moved);
            assert(to_moved == 0);
        }
    }
    while (! other->buf_list_.empty() && other->buf_list_.front().IsEmpty()) {
        other->buf_list_.pop_front();
    }
    
    assert(byte_count_ >= 0 && other->byte_count_ >= 0);
    const int moved = num_bytes - to_moved;
    byte_count_ += moved;
    other->byte_count_ -= moved;

    return moved;
}

void IOBuffer::ZeroFill(int num_bytes)
{
    while(!buf_list_.empty() && buf_list_.back().IsEmpty()) {
        buf_list_.pop_back();
    }

    int nbytes = num_bytes;

    if(nbytes > 0 && ! buf_list_.empty()) {
        nbytes -= buf_list_.back().ZeroFill(nbytes);
    }
    while(nbytes > 0) {
        buf_list_.push_back(IOBufferData());
        nbytes -= buf_list_.back().ZeroFill(nbytes);
    } 
    assert(byte_count_ >= 0);
    if(num_bytes > 0) {
        byte_count_ += num_bytes;
    }
}

int IOBuffer::CopyIn(const char* buf, int num_bytes)
{
    if(! buf || num_bytes < 0) {
        return -1;
    }

    if(buf_list_.empty()) {
        buf_list_.push_back(IOBufferData());
    }
    
    int nbytes = num_bytes;
    const char* cur = buf;
    while(nbytes > 0) {
        if(buf_list_.back().IsFull()) {
            buf_list_.push_back(IOBufferData());
        }
        int nb = buf_list_.back().CopyIn(cur, nbytes);
        cur += nb;
        nbytes -= nb;
        
        assert(nb == 0 || buf_list_.back().IsFull());
    }

    nbytes = num_bytes - nbytes;
    assert(byte_count_ >= 0);
    byte_count_ += nbytes;

    return nbytes;
}


int IOBuffer::Copy(const IOBuffer* other, int num_bytes)
{
    if(! other || num_bytes < 0) {
        return -1;
    }
    int nbytes = num_bytes;
    BList::const_iterator it;
    for(it = other->buf_list_.begin(); it != other->buf_list_.end() && nbytes > 0; it++) {
        int nb = std::min(int(it->BytesConsumable()), nbytes);
        if(nb > 0) {
            char* const c = const_cast<char*>(it->Consumer());
            buf_list_.push_back(IOBufferData(*it, c, c + nb));
        } else {
            continue;
        }
    }
    
    nbytes = num_bytes - nbytes;
    assert(byte_count_ >= 0);
    byte_count_ += nbytes;
    return nbytes;
}

int IOBuffer::CopyOut(char* buf, int num_bytes) const
{
    if(! buf || num_bytes < 0) {
       return -1;
    } 

    char* cur = buf;
    BList::const_iterator it;
    int nbytes = num_bytes;

    if(nbytes > 0) {
        *cur = '\0';
    }
    for(it = buf_list_.begin(); nbytes > 0 && it != buf_list_.end(); it++) {
        const int nb = it->CopyOut(cur, nbytes);
        cur += nb;
        nbytes -= nb;
    }

    return (cur - buf);

}

int IOBuffer::Consume(int num_bytes)
{
    if(num_bytes >= byte_count_) {
        buf_list_.clear();
        const int nbytes = byte_count_;
        byte_count_ = 0;
        return nbytes;
    }

    int nbytes = num_bytes;
    BList::iterator it = buf_list_.begin();
    while(nbytes > 0 && it != buf_list_.end()) {
        nbytes -= it->Consume(nbytes);
        if(it->IsEmpty()) {
            it = buf_list_.erase(it);
        } else {
            it++;
        }
    }

    nbytes = num_bytes - nbytes;
    assert(byte_count_ >= 0);
    byte_count_ -= nbytes;
    
    return nbytes;
}

inline static void* AllocaBuffer(size_t alloc_size)
{
    return s_io_buffer_allocator ? 
        s_io_buffer_allocator->Allocate() : new char[alloc_size];
}

int IOBuffer::Trim(int num_bytes)
{
    if(num_bytes >= byte_count_) {
        return byte_count_;
    }

    if(num_bytes <= 0) {
        buf_list_.clear();
        byte_count_ = 0;
        return byte_count_;
    }

    int nbytes = num_bytes;
    BList::iterator it = buf_list_.begin();

    while(it != buf_list_.end()) {
        const int nb = it->BytesConsumable();
        if(nb <= 0) {
            it = buf_list_.erase(it);
        } else {
            if(nb > nbytes) {
                it->Trim(nbytes);
                if( ! it->IsEmpty()) {
                    ++it;
                }
                break;
            }
            nbytes -= nb;
            it++;
        }
    }

    it = buf_list_.erase(it, buf_list_.end());
    assert(byte_count_ >= 0);
    byte_count_ = num_bytes;
    return byte_count_;
}

int IOBuffer::Read(int fd, int max_read_ahead)
{
    if(s_io_buffer_allocator && ! s_used_io_buffer_allocator) {
        IOBufferData init_with_allocator;
    } 

    const size_t buf_size = s_io_buffer_allocator ? 
        s_io_buffer_allocator->GetBufferSize() : 
        IOBufferData::default_buffer_size();
    if(max_read_ahead > 0 && max_read_ahead <= int(buf_size)) {
        if(buf_list_.empty()) {
            buf_list_.push_back(IOBufferData());
        }
        if(buf_list_.back().SpaceAvailable() >= size_t(max_read_ahead)) {
            const int nread = buf_list_.back().Read(fd, max_read_ahead);
            if(nread > 0) {
                byte_count_ += nread;
            } 
            return nread;
        }
    }

    const ssize_t kMaxReadv = 64 << 10;
    const int kMaxReadvNum(kMaxReadv / (4 << 10) + 1);
    const int max_readv_num = std::min(IOV_MAX,
            std::min(kMaxReadvNum, int(kMaxReadv / buf_size + 1))); 
    struct iovec read_iov[kMaxReadvNum];
    ssize_t total_read = 0;
    bool use_last = ! buf_list_.empty() && ! buf_list_.back().IsFull();

    ssize_t max_read = (max_read_ahead >= 0 ?
            max_read_ahead : std::numeric_limits<int>::max());
   
    while(max_read > 0) {
        assert(use_last || buf_list_.empty() || buf_list_.back().IsFull());
       
        int nvec = 0;
        ssize_t nread = max_read;
        size_t nbytes(nread);

        if(use_last) {
            IOBufferData buf = buf_list_.back();
            const size_t nb = std::min(nbytes, buf.SpaceAvailable());
            read_iov[nvec].iov_base = buf.Producer();
            read_iov[nvec].iov_len = nb;
            nvec++;
            nbytes -= nb;
        }

        for( ; nbytes > 0 && nvec < max_readv_num; nvec++) {
            const size_t nb = std::min(nbytes, buf_size);
            read_iov[nvec].iov_len = nb;
            if (! (read_iov[nvec].iov_base = AllocaBuffer(nb))) {
                if(total_read <= 0 && nvec <= 0) {
                    abort(); // Allocation failure.
                }
                break;
            }
            nbytes -= nb;
        }
        nread -= nbytes;

        const ssize_t rd = readv(fd, read_iov, nvec);
        if(rd < nread) {
            max_read = 0; // short read, eof or error: we're done.
        } else if (max_read > 0) {
            max_read -= rd;
            assert(max_read >= 0);
        }
        
        nread = std::max(ssize_t(0), rd);
        int i = 0;

        if(use_last) {
            nread -= buf_list_.back().Fill(nread);
            i++;
            use_last = false;
        }
       
        for( ; i < nvec; i++) {
            char* const buf = reinterpret_cast<char*>(read_iov[nvec].iov_base);
            if(nread > 0) {
                if(s_io_buffer_allocator) {
                    buf_list_.push_back(
                        IOBufferData(buf, 0, nread, *s_io_buffer_allocator));
                } else {
                    buf_list_.push_back(
                        IOBufferData(buf, buf_size, 0, nread));
                }
                nread -=  buf_list_.back().BytesConsumable();
            } else {
                if(s_io_buffer_allocator) {
                    s_io_buffer_allocator->Deallocate(buf);
                } else {
                    delete [] buf;
                }
           }
        }
        assert(nread == 0);
        if(rd > 0) {
            total_read += rd;
        } else if(total_read == 0 && rd < 0 &&
            (total_read = (errno == 0 ? EAGAIN : -errno)) > 0) {
            total_read = -total_read;
        }
    }
    assert(byte_count_ >= 0);
    if(total_read > 0) {
        byte_count_ += total_read;
    }

    return total_read;
}

int IOBuffer::Write(int fd)
{
    const int kMaxWritevNum = 32;
    const int max_write_num = std::min(IOV_MAX, kMaxWritevNum);
    const int kPreferredWriteSize = 64 << 10;
    
    struct iovec write_iov[kMaxWritevNum];
    ssize_t total_write = 0;

    while(! buf_list_.empty()) {
        BList::iterator it;
        int nvec;
        ssize_t to_write;
        for(it = buf_list_.begin(), nvec = 0, to_write = 0;
            it != buf_list_.end() && nvec < max_write_num && total_write < kPreferredWriteSize;
            ) {
            const int nbytes = it->BytesConsumable();
            if(nbytes <= 0 ) {
                it = buf_list_.erase(it);
                continue;
            } 
            write_iov[nvec].iov_len = nbytes;
            write_iov[nvec].iov_base = it->Consumer();
            to_write += nbytes;
            nvec++;
            it++;
        }

        if(nvec <= 0) {
            assert(it == buf_list_.end());
            buf_list_.clear();
            break;
        }   

        const ssize_t nw = writev(fd, write_iov, nvec);
        if(nw == to_write && it == buf_list_.end()) {
            buf_list_.clear();
        } else {
            ssize_t to_erase = nw;
            int nb;
            while((nb = buf_list_.front().BytesConsumable()) <= to_erase) {
                to_erase -= nb;
                buf_list_.pop_front();
            }
            if(to_erase > 0) {
                to_erase -= buf_list_.front().Consume(to_erase);
                assert(to_erase == 0);
            }
        }

        if(nw > 0) {
            total_write += nw;
        } else if(total_write <= 0 && (total_write = (errno == 0 ? EAGAIN : -errno)) > 0) {
            total_write = -total_write;
        }

        if(nw != to_write) {
            break;
        }
    }

    assert(byte_count_ >= 0);
    if(total_write > 0) {
        assert(byte_count_ >= total_write);
        byte_count_ -= total_write;
    }
            
    return total_write;
}

} // namespace paxoslease
