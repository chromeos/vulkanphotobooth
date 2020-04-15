/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef VULKAN_PHOTO_BOOTH_RING_BUFFER_H
#define VULKAN_PHOTO_BOOTH_RING_BUFFER_H

#include <cstdlib>
#include <memory>
#include <mutex>
#include "vulkan-utils/vulkan_utils.h"

/**
 * Standard ring buffer of type T
 *
 * @tparam T type for the ring buffer
 */
template <class T>
class RingBuffer {
    public:
        RingBuffer(size_t size) :
                buffer(std::unique_ptr<T[]>(new T[size])),
                CAPACITY(size) {
        }

        void put(T item) {
            std::lock_guard<std::mutex> lock(mutex);

            // Add item, replace previous
            buffer[head] = item;

            // Advance front of buffer
            head = (head + 1) % CAPACITY;

            // If we're full, also advance tail
            if(isFull()) {
                tail = (tail + 1) % CAPACITY;
            } else {
                num_items++;
            }
        }

        T get() {
            std::lock_guard<std::mutex> lock(mutex);
            if(isEmpty()) {
                return T();
            }

            // Return data, advance tail ("forgetting" about data)
            auto val = buffer[tail];
            num_items--;
            tail = (tail + 1) % CAPACITY;

            return val;
        }

        bool isEmpty() const {
            return (0 == num_items);
        }

        bool isFull() const {
            return (CAPACITY == num_items);
        }

        size_t numItems() const {
            return num_items;
        }

        const size_t CAPACITY;

private:
        std::mutex mutex;
        std::unique_ptr<T[]> buffer;
        size_t num_items = 0;
        size_t head = 0;
        size_t tail = 0;
};

#endif //VULKAN_PHOTO_BOOTH_RING_BUFFER_H