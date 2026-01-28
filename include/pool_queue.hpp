#pragma once
#include <cstddef>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

template <typename T, size_t N>
class PoolQueue {
public:
    PoolQueue() = default;

    bool init() {
        freeQ_ = xQueueCreate(N, sizeof(T*));
        dataQ_ = xQueueCreate(N, sizeof(T*));
        if (!freeQ_ || !dataQ_) return false;

        // Load freeQ_ with pointers to pool_
        for (size_t i = 0; i < N; i++) {
            T* p = &pool_[i];
            if (xQueueSend(freeQ_, &p, 0) != pdTRUE) return false;
        }
        return true;
    }

    // RAII handle for a slot acquired from freeQ_
    class Slot {
    public:
        Slot() = default;
        Slot(PoolQueue* owner, T* ptr) : owner_(owner), ptr_(ptr) {}

        // non-copyable
        Slot(const Slot&) = delete;
        Slot& operator=(const Slot&) = delete;

        // movable
        Slot(Slot&& other) noexcept { *this = std::move(other); }
        Slot& operator=(Slot&& other) noexcept {
            if (this != &other) {
                cleanup(); // release our current if any
                owner_ = other.owner_;
                ptr_ = other.ptr_;
                published_ = other.published_;
                other.owner_ = nullptr;
                other.ptr_ = nullptr;
                other.published_ = false;
            }
            return *this;
        }

        ~Slot() { cleanup(); }

        T* get() { return ptr_; }
        T& operator*() { return *ptr_; }
        T* operator->() { return ptr_; }
        explicit operator bool() const { return ptr_ != nullptr; }

        // publish to dataQ_ (transfers ownership to consumer)
        bool publish(TickType_t to = portMAX_DELAY) {
            if (!owner_ || !ptr_) return false;
            T* p = ptr_;
            if (xQueueSend(owner_->dataQ_, &p, to) != pdTRUE) {
                return false; // still owned by Slot, destructor will release back to freeQ_
            }
            published_ = true;
            ptr_ = nullptr; // no longer ours
            return true;
        }

    private:
        void cleanup() {
            // If we still own a pointer and did not publish, return it to freeQ_
            if (owner_ && ptr_ && !published_) {
                T* p = ptr_;
                xQueueSend(owner_->freeQ_, &p, 0); // should succeed in correct design
            }
            owner_ = nullptr;
            ptr_ = nullptr;
            published_ = false;
        }

        PoolQueue* owner_ = nullptr;
        T* ptr_ = nullptr;
        bool published_ = false;
    };

    // Acquire a free slot for producer use
    Slot acquire(TickType_t to = portMAX_DELAY) {
        T* p = nullptr;
        if (!freeQ_ || xQueueReceive(freeQ_, &p, to) != pdTRUE) {
            return Slot{};
        }
        return Slot{this, p};
    }

    // Consumer side (we’ll RAII this next)
    bool receive(T** out, TickType_t to = portMAX_DELAY) {
        return dataQ_ && xQueueReceive(dataQ_, out, to) == pdTRUE;
    }
    bool release(T* p) {
        if (!freeQ_ || !p) return false;
        return xQueueSend(freeQ_, &p, 0) == pdTRUE;
    }

    QueueHandle_t dataQ() const { return dataQ_; }
    QueueHandle_t freeQ() const { return freeQ_; }

private:
    QueueHandle_t freeQ_ = nullptr;
    QueueHandle_t dataQ_ = nullptr;
    T pool_[N]{};
};
