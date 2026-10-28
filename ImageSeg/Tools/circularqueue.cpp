#include "circularqueue.h"


//template<typename T>
//CircularQueue<T>::CircularQueue(size_t max_size, bool overwrite)
//    : max_size_(max_size > 0 ? max_size : 1000)
//    , overwrite_mode_(overwrite)
//    , shutdown_flag_(false) {
//}

//template<typename T>
//bool CircularQueue<T>::push(const T& item) {
//    std::unique_lock<std::mutex> lock(mutex_);

//    if (shutdown_flag_.load()) {
//        return false;
//    }

//    // 如果队列已满且启用覆盖模式，则移除头部元素
//    if (queue_.size() >= max_size_ && overwrite_mode_.load()) {
//        queue_.pop();  // 移除最老的元素
//    }

//    // 如果队列仍然满（可能因为max_size=0或覆盖模式被禁用），等待空间
//    if (queue_.size() >= max_size_) {
//        not_full_.wait(lock, [this]() {
//            return queue_.size() < max_size_ || shutdown_flag_.load();
//        });

//        if (shutdown_flag_.load()) {
//            return false;
//        }
//    }

//    queue_.push(item);
//    lock.unlock();
//    not_empty_.notify_one();
//    return true;
//}

//template<typename T>
//bool CircularQueue<T>::push(T&& item) {
//    std::unique_lock<std::mutex> lock(mutex_);

//    if (shutdown_flag_.load()) {
//        return false;
//    }

//    // 如果队列已满且启用覆盖模式，则移除头部元素
//    if (queue_.size() >= max_size_ && overwrite_mode_.load()) {
//        queue_.pop();  // 移除最老的元素
//    }

//    // 如果队列仍然满，等待空间
//    if (queue_.size() >= max_size_) {
//        not_full_.wait(lock, [this]() {
//            return queue_.size() < max_size_ || shutdown_flag_.load();
//        });

//        if (shutdown_flag_.load()) {
//            return false;
//        }
//    }

//    queue_.push(std::move(item));
//    lock.unlock();
//    not_empty_.notify_one();
//    return true;
//}

//template<typename T>
//std::optional<T> CircularQueue<T>::pop() {
//    std::unique_lock<std::mutex> lock(mutex_);

//    if (queue_.empty() || shutdown_flag_.load()) {
//        return std::nullopt;
//    }

//    T item = std::move(queue_.front());
//    queue_.pop();
//    lock.unlock();
//    not_full_.notify_one();
//    return item;
//}

//template<typename T>
//std::optional<T> CircularQueue<T>::pop_wait(std::chrono::milliseconds timeout) {
//    std::unique_lock<std::mutex> lock(mutex_);

//    // 等待队列有数据或超时或收到关闭信号
//    if (timeout.count() > 0) {
//        bool wait_result = not_empty_.wait_for(lock, timeout, [this]() {
//            return !queue_.empty() || shutdown_flag_.load();
//        });

//        if (!wait_result || shutdown_flag_.load() || queue_.empty()) {
//            return std::nullopt;
//        }
//    } else {
//        not_empty_.wait(lock, [this]() {
//            return !queue_.empty() || shutdown_flag_.load();
//        });

//        if (shutdown_flag_.load() || queue_.empty()) {
//            return std::nullopt;
//        }
//    }

//    T item = std::move(queue_.front());
//    queue_.pop();
//    lock.unlock();
//    not_full_.notify_one();
//    return item;
//}

//template<typename T>
//bool CircularQueue<T>::empty() const {
//    std::lock_guard<std::mutex> lock(mutex_);
//    return queue_.empty();
//}

//template<typename T>
//bool CircularQueue<T>::full() const {
//    std::lock_guard<std::mutex> lock(mutex_);
//    return queue_.size() >= max_size_;
//}

//template<typename T>
//size_t CircularQueue<T>::size() const {
//    std::lock_guard<std::mutex> lock(mutex_);
//    return queue_.size();
//}

//template<typename T>
//size_t CircularQueue<T>::capacity() const {
//    return max_size_;
//}

//template<typename T>
//void CircularQueue<T>::clear() {
//    std::lock_guard<std::mutex> lock(mutex_);
//    while (!queue_.empty()) {
//        queue_.pop();
//    }
//    not_full_.notify_all();
//}

//template<typename T>
//void CircularQueue<T>::shutdown() {
//    shutdown_flag_.store(true);
//    not_empty_.notify_all();
//    not_full_.notify_all();
//}
