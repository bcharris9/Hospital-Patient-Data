#include "BoundedBuffer.h"

using namespace std;


BoundedBuffer::BoundedBuffer (int _cap) : cap(_cap) {
    // modify as needed
}

BoundedBuffer::~BoundedBuffer () {
    // modify as needed
}

void BoundedBuffer::push (char* msg, int size) {
    // 1. Convert the incoming byte sequence given by msg and size into a vector<char>
    // 2. Wait until there is room in the queue (i.e., queue lengh is less than cap)
    // 3. Then push the vector at the end of the queue
    // 4. Wake up threads that were waiting for push
    
    vector<char> vecy(msg, msg + size);
    std::unique_lock<std::mutex> lock(m);
    cv_push.wait(lock, [this]() { return static_cast<int>(q.size()) < cap; });
    q.push(vecy);
    lock.unlock();
    cv_pop.notify_one();

}

int BoundedBuffer::pop (char* msg, int size) {
    // 1. Wait until the queue has at least 1 item
    // 2. Pop the front item of the queue. The popped item is a vector<char>
    // 3. Convert the popped vector<char> into a char*, copy that into msg; assert that the vector<char>'s length is <= size
    // 4. Wake up threads that were waiting for pop
    // 5. Return the vector's length to the caller so that they know how many bytes were popped
    
    std::unique_lock<std::mutex> lock(m);
    cv_pop.wait(lock, [this]() { return !q.empty(); });
    std::vector<char> vecy = q.front();
    q.pop();
    assert(vecy.size() <= static_cast<size_t>(size));
    std::copy(vecy.data(), vecy.data() + vecy.size(), msg);
    lock.unlock();
    cv_push.notify_one();
    return vecy.size();
}

size_t BoundedBuffer::size () {
    return q.size();
}

