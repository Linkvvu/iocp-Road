#pragma once
#include <functional>
#include <memory>

class Session;
class Buffer;

// todo: onDisConnectedCallback

using shared_session_ptr      = std::shared_ptr<Session>;
using onConnectedCallback     = std::function<void(shared_session_ptr)>;
using onMessageCallback       = std::function<void(shared_session_ptr, Buffer* buffer)>;
using onSendCompletedCallback = std::function<void(shared_session_ptr)>;