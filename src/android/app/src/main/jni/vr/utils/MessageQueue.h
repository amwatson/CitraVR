/*******************************************************************************

Filename    :   MessageQueue.h
Content     :   Non-blocking queue for passing messages to the render thread.
Authors     :   Amanda M. Watson
License     :   Licensed under GPLv3 or any later version.
                Refer to the license.txt file included.

*******************************************************************************/

#pragma once

#include <mutex>
#include <queue>

#include <cstdint>

/**
 * Message class
 *
 * Represents a message to be passed to the render thread.
 * Optionally contains a payload, which is interpreted based on the message
 * type.
 */
struct Message {
    enum Type { SHOW_KEYBOARD = 0 };

    Message() {}
    Message(const Type type)
        : mType(type) {}
    Message(const Type type, const uint64_t payload)
        : mType(type)
        , mPayload(payload) {}

    Type     mType;
    uint64_t mPayload;
};

/**
 * Message queue class
 */
class MessageQueue {
public:
    MessageQueue() = default;
    ~MessageQueue();

    /**
     * Push a message onto the queue
     * @param msg The message to push
     * @return void
     */
    void Post(const Message& msg);
    /**
     * Pop a message off the queue.
     * @param msg The message to pop
     * @return bool False if the queue is empty and no message was popped, true
     * if a message was popped/returned.
     */
    bool Poll(Message& msg);

private:
    // Message queue
    std::queue<Message> mQueue;
    // Mutex for the message queue
    std::mutex mQueueMutex;
};
