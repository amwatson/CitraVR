/*******************************************************************************

Filename    :   MessageQueue.cpp
Content     :   Non-blocking queue for passing messages to the render thread.
Authors     :   Amanda M. Watson
License     :   Licensed under GPLv3 or any later version.
                Refer to the license.txt file included.

*******************************************************************************/

#include "MessageQueue.h"

#include <jni.h>
#include <stdlib.h>

MessageQueue::~MessageQueue()
{
    std::unique_lock<std::mutex> lock(mQueueMutex);
    mQueue = std::queue<Message>();
}

void MessageQueue::Post(const Message& message)
{
    std::unique_lock<std::mutex> lock(mQueueMutex);
    mQueue.push(message);
}

bool MessageQueue::Poll(Message& message)
{
    std::unique_lock<std::mutex> lock(mQueueMutex);
    if (mQueue.empty()) { return false; }
    message = mQueue.front();
    mQueue.pop();
    return true;
}
