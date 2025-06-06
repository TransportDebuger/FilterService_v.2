#include "../include/workercontainer.hpp"

template<typename Func>
void WorkersContainer::access(Func&& func) {
    std::lock_guard lock(mutex_);
    func(workers_);
}

size_t WorkersContainer::size() const {
    std::lock_guard lock(mutex_);
    return workers_.size();
}

void WorkersContainer::swap(WorkersContainer& other) {
    using std::swap;
    std::unique_lock lock1(mutex_, std::defer_lock);
    std::unique_lock lock2(other.mutex_, std::defer_lock);
    std::lock(lock1, lock2);
    
    swap(workers_, other.workers_);
}