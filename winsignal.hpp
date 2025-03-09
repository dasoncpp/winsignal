/**
 * @file    winsignal.hpp
 * @brief   qt-like signal and slot library for windows
 * - based on Windows message mechanism and event loop
 * @author  lindasheng
 * @date    2025-02-07
 * @version 1.0.0
 *
 * @copyright Copyright (c) 2025 baioo
 *            Released under the MIT License 2.0
 *
 * @warning
 * - required C++ 17 
 * - msvc only
 */

#ifndef __WIN_SIGNAL_HPP__
#define __WIN_SIGNAL_HPP__

#include <tuple>
#include <functional>
#include <type_traits>
#include <map>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <thread>
#include <queue>
#include <shared_mutex>
#include <memory>
#include <iostream>
#include <Windows.h>

namespace winSignal
{
    enum class ConnectionType
    {
        AutoConnection,
        DirectConnection,
        QueuedConnection,
        BlockingQueuedConnection,
    };

    class WeakFlag;
    class Object;
    class EventLoopObject;
    class Timer;

    template<typename ...Args>
    class Signal;

    class EventLoop;

    static EventLoop *GetEventLoop(std::thread::id id = std::this_thread::get_id());

    static EventLoop *GetEventLoop(const std::thread &thread);

    template<typename Sender, typename Receiver, typename T, typename U, typename ...SignalArgs, typename ...SlotArgs>
    static constexpr void Connect(Sender *sender, Signal<SignalArgs...> T::* event, Receiver *receiver, void(U::* handler)(SlotArgs...), ConnectionType type = ConnectionType::AutoConnection);

    template<typename Sender, typename Receiver, typename T, typename U, typename ...SignalArgs, typename ...SlotArgs>
    static constexpr void Disconnect(Sender *sender, Signal<SignalArgs...> T::* event, Receiver *receiver, void(U::* handler)(SlotArgs...));

    template<typename Sender, typename Receiver, typename T, typename U, typename ...SignalArgs, typename ...SlotArgs>
    static constexpr void Connect(Sender *sender, Signal<SignalArgs...> T::* event, Receiver *receiver, void(U::* handler)(SlotArgs...) const, ConnectionType type = ConnectionType::AutoConnection);

    template<typename Sender, typename Receiver, typename T, typename U, typename ...SignalArgs, typename ...SlotArgs>
    static constexpr void Disconnect(Sender *sender, Signal<SignalArgs...> T::* event, Receiver *receiver, void(U::* handler)(SlotArgs...) const);

    template<typename Sender, typename Receiver, typename T, typename Lambda, typename ...SignalArgs>
    static constexpr void Connect(Sender *sender, Signal<SignalArgs...> T::* event, Receiver *receiver, Lambda &&lambda, ConnectionType type = ConnectionType::AutoConnection);

    template<typename Sender, typename T, typename Lambda, typename ...SignalArgs>
    static constexpr void Connect(Sender* sender, Signal<SignalArgs...> T::* event, Lambda&& lambda);

    template<typename Sender, typename T, typename ...SignalArgs, typename ...SlotArgs>
    static constexpr void Connect(Sender *sender, Signal<SignalArgs...> T::* event, void(*handler)(SlotArgs...));

    template<typename Sender, typename T, typename ...SignalArgs, typename ...SlotArgs>
    static constexpr void Disconnect(Sender *sender, Signal<SignalArgs...> T::* event, void(*handler)(SlotArgs...));

}
namespace winSignal::Implementation
{
    template<typename Subset, typename Superset>
    struct is_subset_of;

    template<typename T, typename ...Subset, typename U, typename ...Superset>
    struct is_subset_of<std::tuple<T, Subset...>, std::tuple<U, Superset...>>
    {
        constexpr static bool value = is_subset_of<std::tuple<T, Subset...>, std::tuple<Superset...>>::value;
    };

    template<typename T, typename ...Subset, typename ...Superset>
    struct is_subset_of<std::tuple<T, Subset...>, std::tuple<T, Superset...>>
    {
        constexpr static bool value = is_subset_of<std::tuple<Subset...>, std::tuple<Superset...>>::value;
    };

    template<typename ...Superset>
    struct is_subset_of<std::tuple<>, std::tuple<Superset...>>
    {
        constexpr static bool value = true;
    };

    template<typename ...Subset>
    struct is_subset_of<std::tuple<Subset...>, std::tuple<>>
    {
        constexpr static bool value = false;
    };

    template<>
    struct is_subset_of<std::tuple<>, std::tuple<>>
    {
        constexpr static bool value = true;
    };

    template<int N, typename T, typename Tuple>
    struct find_next_index;

    template<int N, typename T, typename U, typename ...Args>
    struct find_next_index<N, T, std::tuple<U, Args...>>
    {
        constexpr static int value = find_next_index<N - 1, T, std::tuple<Args...>>::value + 1;
    };
    template<typename T, typename ...Args>
    struct find_next_index<-1, T, std::tuple<T, Args...>>
    {
        constexpr static int value = 0;
    };
    template<typename T, typename U, typename ...Args>
    struct find_next_index<-1, T, std::tuple<U, Args...>>
    {
        constexpr static int value = find_next_index<-1, T, std::tuple<Args...>>::value + 1;
    };

    template<int ...Args>
    struct int_list
    {
        using type = int_list<Args...>;

        template<typename Tuple>
        constexpr static auto MakeTupleByList(const Tuple &target) noexcept
        {
            return std::make_tuple(std::get<Args>(target)...);
        }
    };

    template<int N, typename list>
    struct list_prepend;

    template<int N, int ...Args>
    struct list_prepend<N, int_list<Args...>>
    {
        using result = int_list<N, Args...>;
    };

    template<int N, typename Subset, typename Superset>
    struct find_all_index;

    template<int N, typename T, typename ...Subset, typename ...Superset>
    struct find_all_index<N, std::tuple<T, Subset...>, std::tuple<Superset...>>
    {
        using value = typename list_prepend<find_next_index<N, T, std::tuple<Superset...>>::value,
        typename find_all_index<find_next_index<N, T, std::tuple<Superset...>>::value, std::tuple<Subset...>, std::tuple<Superset...>>::value>::result;
    };

    template<int N, typename ...Superset>
    struct find_all_index<N, std::tuple<>, std::tuple<Superset...>>
    {
        using value = int_list<>;
    };


    template<typename ...Subset, typename ...Superset>
    constexpr auto TupleTake(const std::function<void(Subset...)> &func, const std::tuple<Superset...> &target) noexcept
    {
        using SubsetTuple = std::tuple<std::decay_t<Subset>...>;
        using SupersetTuple = std::tuple<std::decay_t<Superset>...>;

        static_assert(is_subset_of<SubsetTuple, SupersetTuple>::value, "slot function parameters and signal parameters do not match");
        if constexpr (is_subset_of<SubsetTuple, SupersetTuple>::value)
        {
            using IndexList = typename find_all_index<-1, SubsetTuple, SupersetTuple>::value;
            std::apply(func, IndexList::MakeTupleByList(target));
        }
    }

    template<typename T, typename U>
    struct is_object
    {
        constexpr static bool value = false;
    };

    template<typename T>
    struct is_object<T, decltype(std::declval<T>().ThreadId())>
    {
        constexpr static bool value = true;
    };

    template<typename ...Args>
    class EventHandlerInterface
    {
    public:
        EventHandlerInterface(const EventHandlerInterface &) = delete;
        EventHandlerInterface &operator=(const EventHandlerInterface &event) = delete;
        EventHandlerInterface() = default;
        virtual ~EventHandlerInterface() = default;

        virtual void operator()(const Args &...args) = 0;
    };

    template<typename T, typename Tuple, typename ...Args>
    class EventHandler final : public EventHandlerInterface<Args...> {};

    template<typename ...SlotArgs, typename ...Args>
    class EventHandler<void, std::tuple<SlotArgs...>, Args...> final : public EventHandlerInterface<Args...>
    {
    private:
        std::function<void(SlotArgs...)> m_Handler;
    public:
        EventHandler(const EventHandler &eventHandler) = delete;
        EventHandler &operator=(const EventHandler &eventHandler) = delete;

        template<typename Callable>
        explicit EventHandler(Callable &&func) noexcept
        {
            m_Handler = std::forward<Callable>(func);
        }

        void operator()(const Args &... args) final
        {
            TupleTake(m_Handler, std::make_tuple(args...));
        }
    };


    template<typename T, typename ...SlotArgs, typename ...Args>
    class EventHandler<T, std::tuple<SlotArgs...>, Args...> final : public EventHandlerInterface<Args...>
    {
        using FunctionPointer = void (T::*)(SlotArgs...);
        using ConstFunctionPointer = void (T::*)(SlotArgs...) const;
    private:
        T *m_Receiver;
        std::function<void(T *, SlotArgs...)> m_Handler;
    public:
        EventHandler(const EventHandler &eventHandler) = delete;
        EventHandler &operator=(const EventHandler &eventHandler) = delete;

        EventHandler(T *receiver, FunctionPointer handler) noexcept
        {
            m_Receiver = receiver;
            m_Handler = handler;
        }

        EventHandler(T *receiver, ConstFunctionPointer handler) noexcept
        {
            m_Receiver = receiver;
            m_Handler = handler;
        }

        void operator()(const Args &...args) final
        {
            TupleTake(m_Handler, std::make_tuple(m_Receiver, args...));
        }
    };

    struct ClassFunctionPointer
    {
        void *address = nullptr;

        bool operator==(const ClassFunctionPointer &other) const
        {
            return address == other.address;
        }

        bool operator<(const ClassFunctionPointer &other) const
        {
            return address < other.address;
        }
    };

    struct Address
    {
        void *object;
        ClassFunctionPointer function;

        template<typename T, typename U, typename ...Args>
        constexpr Address(T *receiver, void(U::* handler)(Args...)) noexcept
        {
            object = receiver;
            memcpy(&function, &handler, sizeof(ClassFunctionPointer));
        }

        template<typename T, typename U,typename ...Args>
        constexpr Address(T *receiver, void(U::* handler)(Args...) const) noexcept
        {
            object = receiver;
            memcpy(&function, &handler, sizeof(ClassFunctionPointer));
        }

        template<typename T,typename U, typename ...Args>
        constexpr Address(T *sender, Signal<Args...> U::* event) noexcept
        {
            object = sender;
            memcpy(&function, &event, sizeof(event));
        }

        template<typename ...Args>
        constexpr explicit Address(void(*handler)(Args...)) noexcept
        {
            object = nullptr;
            memcpy(&function, &handler, sizeof(void (*)()));
        }

        bool operator==(const Address &other) const noexcept
        {
            return other.object == object && other.function == function;
        }

    };

    struct AddressHash
    {
        std::size_t operator()(const Address &address) const
        {
            std::size_t h1 = std::hash<void *>{}(address.object);
            constexpr static bool condition =(sizeof(void (Address::*)()) == sizeof(void *));
            static_assert(condition, "winSignal can't work ");
            if constexpr (sizeof(address.function) == sizeof(void *))
            {
                std::size_t h2 = std::hash<void *>{}((void *) address.function.address);
                return h1 ^ (h2 << 1);
            }
        }
    };

    using FunctionPointer = ClassFunctionPointer;

    template<typename Callable>
    static void AddSender(Object &object, const Address &senderAddress, const FunctionPointer &functionAddress, Callable &&func);

    template<typename Callable>
    static void AddReceiver(Object &object, const Address &receiverAddress, Callable &&func);

    static void RemoveSender(Object &object, const Address &senderAddress, const FunctionPointer &functionAddress);

    static void RemoveReceiver(Object &object, const Address &address);

    static bool ContainSender(const Object &object, const Address &address);

    static bool ContainReceiver(const Object &object, const Address &address);

    class EventLoopManager
    {
    private:
        std::unordered_map<std::thread::id, EventLoop *> m_EventLoops;
        std::mutex m_Mutex;
        EventLoopManager() = default;
        ~EventLoopManager() = default;

    public:
        EventLoopManager(const EventLoopManager &) = delete;
        EventLoopManager &operator=(const EventLoopManager &) = delete;

        static EventLoopManager *GetInstance() noexcept
        {
            static EventLoopManager instance;
            return &instance;
        }

        void AddEventLoop(EventLoop *loop)
        {
            std::unique_lock<std::mutex> lock(m_Mutex);
            m_EventLoops.insert(std::make_pair(std::this_thread::get_id(), loop));
        }

        void RemoveEventLoop()
        {
            std::unique_lock<std::mutex> lock(m_Mutex);
            if (m_EventLoops.count(std::this_thread::get_id()))
            {
                m_EventLoops.erase(std::this_thread::get_id());
            }
        }

        EventLoop *GetEventLoop(std::thread::id id)
        {
            std::unique_lock<std::mutex> lock(m_Mutex);
            if (m_EventLoops.count(id))
            {
                return m_EventLoops[id];
            }
            return nullptr;
        }
    };

}
namespace winSignal
{
    class EventLoop
    {
    private:
        std::mutex m_Mutex;
        std::deque<std::function<void()>> m_Messages;
        std::unordered_map<UINT_PTR, std::function<void()>> m_SingleShotTimerProcs;
        std::unordered_map<UINT_PTR, std::function<void()>> m_RepeatTimerProcs;
        HWND m_WndHandle{};
        std::atomic<int> m_TimerIdAutoIncrease = WM_USER;
        const int m_MsgId = WM_USER + 1001;
#ifdef UNICODE
        const std::wstring m_WndClassName = L"ONESDK_InternalMessageWindow";
#else
        const std::string m_WndClassName = "ONESDK_InternalMessageWindow";
#endif // UNICODE

    public:
        EventLoop()
        {
            CreateInternalWindow();
            Implementation::EventLoopManager::GetInstance()->AddEventLoop(this);
        }

        ~EventLoop()
        {
            Implementation::EventLoopManager::GetInstance()->RemoveEventLoop();
        }

        bool CreateInternalWindow()
        {
            WNDCLASSEX wc = { 0 };
            wc.cbSize = sizeof(wc);
            wc.lpfnWndProc = WndProc;
            wc.hInstance = GetModuleHandle(nullptr);
            wc.lpszClassName = m_WndClassName.c_str();
            RegisterClassEx(&wc);

            m_WndHandle = CreateWindow(m_WndClassName.c_str(), m_WndClassName.c_str(), WS_OVERLAPPED, 0, 0, 0, 0,
                HWND_MESSAGE, nullptr, GetModuleHandle(nullptr), nullptr);

            if (m_WndHandle == nullptr)
            {
                return false;
            }

            ::SetWindowLongPtr(m_WndHandle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
            return true;
        }
        
        static LRESULT WINAPI WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
        {
            EventLoop* pThis = reinterpret_cast<EventLoop*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
            if (pThis && message == pThis->m_MsgId)
            {
                pThis->HandlerMessage();
            }
            else
            {
                switch (message)
                {
                case WM_NCDESTROY:
                    ::SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(nullptr));
                    ::PostQuitMessage(0);
                    break;
                case WM_TIMER:
                    pThis->HandlerTimer(wParam);
                    break;
                }
            }
            return DefWindowProc(hWnd, message, wParam, lParam);
        }

        void HandlerMessage()
        {
            std::deque<std::function<void()>> messages;
            {
                std::unique_lock<std::mutex> lock(m_Mutex);
                messages = m_Messages;
                m_Messages.clear();
            }
            while (!messages.empty()) {
                std::function<void()> func = std::move(messages.front());
                messages.pop_front();
                func();
            }
        }

        void HandlerTimer(UINT_PTR timerId)
        {
            auto iter = m_SingleShotTimerProcs.find(timerId);
            if (iter != m_SingleShotTimerProcs.end())
            {
               iter->second();
               ::KillTimer(m_WndHandle, timerId);
               m_SingleShotTimerProcs.erase(iter);
               return;
            }

            iter = m_RepeatTimerProcs.find(timerId);
            if (iter != m_RepeatTimerProcs.end())
            {
                iter->second();
            }
        }

        template<typename Callable>
        void SetSingleShotTimer(int interval, Callable&& func)
        {
            PostEvent([=]() {
                if (interval == 0)
                {
                    func();
                    return;
                }
                auto id = ::SetTimer(m_WndHandle, m_TimerIdAutoIncrease++, interval, nullptr);
                m_SingleShotTimerProcs[id] = func;
            });
        }

        template<typename Callable>
        UINT_PTR SetRepeatTimer(int interval, Callable&& func)
        {
            SendEvent([=]() {
                auto id = ::SetTimer(m_WndHandle, m_TimerIdAutoIncrease, interval, nullptr);
                m_RepeatTimerProcs[id] = func;
            });
            return m_TimerIdAutoIncrease++;
        }

        void KillTimer(UINT_PTR timerId)
        {
            PostEvent([=]() {
                auto iter = m_RepeatTimerProcs.find(timerId);
                if (iter != m_RepeatTimerProcs.end())
                {
                    ::KillTimer(m_WndHandle, timerId);
                    m_RepeatTimerProcs.erase(iter);
                }
            });
        }

        template<typename Callable>
        void PostEvent(Callable &&func)
        {
            std::unique_lock<std::mutex> lock(m_Mutex);
            m_Messages.push_back(std::forward<Callable>(func));
            ::PostMessage(m_WndHandle, m_MsgId, NULL, NULL);
        }

        template<typename Callable>
        void SendEvent(Callable &&func)
        {
            {
                std::unique_lock<std::mutex> lock(m_Mutex);
                m_Messages.push_back(std::forward<Callable>(func));
            }
            ::SendMessage(m_WndHandle, m_MsgId, NULL, NULL);
        }

        void Run()
        {
            MSG msg;
            BOOL bRet;
            while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0)
            {
                if (bRet == -1)
                {
                    // handle the error and possibly exit
                }
                else
                {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
            }
        }

        void Quit()
        {
            if (m_WndHandle)
            {
                ::DestroyWindow(m_WndHandle);
                m_WndHandle = nullptr;
            }
        }
    };

    class Thread
    {
    private:
        std::atomic<std::thread::id> id;
        std::thread thread;
    public:
        Thread(const Thread &other) = delete;
        Thread &operator=(const Thread &other) = delete;

        Thread(Thread &&other) noexcept
        {
            id = other.id.load();
            thread = std::move(other.thread);
        }

        Thread &operator=(Thread &&other) noexcept
        {
            id = other.id.load();
            thread = std::move(other.thread);
            return *this;
        }

        template<typename Callable, typename ...Args>
        explicit Thread(Callable &&func, Args &&... args) :thread([&]()
        {
            id = std::this_thread::get_id();
            std::forward<Callable>(func)(std::forward<Args>(args)...);
        })
        {
            while (GetEventLoop(id) == nullptr)
            {
                std::this_thread::yield();
            }
            thread.detach();
        }

        std::thread::id GetID() const
        {
            return id.load();
        }
    };

    template<typename ...Args>
    class Signal
    {
    private:
        using Address = Implementation::Address;
        using AddressHash = Implementation::AddressHash;
        struct Handler
        {
            std::thread::id id;
            std::shared_ptr<Implementation::EventHandlerInterface<Args...>> handler;
            ConnectionType type{};
        };
    private:
        std::unordered_map<Address, Handler, AddressHash> m_Handlers;
        mutable std::shared_mutex m_Mutex;
        std::shared_ptr<WeakFlag> m_weakFlag;
    private:
        void AddHandler(const Address &address, const Handler &handler)
        {
            std::unique_lock<std::shared_mutex> lock(m_Mutex);
            if (!m_Handlers.count(address))
            {
                m_Handlers.insert(std::make_pair(address, handler));
            }
        }

        void RemoveHandler(const Address &address)
        {
            std::unique_lock<std::shared_mutex> lock(m_Mutex);
            if (m_Handlers.count(address))
            {
                m_Handlers.erase(address);
            }
        }

        template<typename T, typename U, typename ...SlotArgs>
        Address ImitationFunctionHelper(T *object, U &&t, void(std::decay_t<U>::* func)(SlotArgs...), ConnectionType type)
        {
            constexpr bool is_object_v = Implementation::is_object<T,std::thread::id>::value;
            Address address = Address(object, func);
            Handler v_handler;
            v_handler.id = std::this_thread::get_id();
            v_handler.handler = std::make_shared<Implementation::EventHandler<void, std::tuple<SlotArgs...>, Args...>>(std::forward<U>(t));
            v_handler.type = type;
            if constexpr (is_object_v)
            {
                v_handler.id = object->ThreadId();
            }
            AddHandler(address, v_handler);
            return address;
        }

        template<typename T, typename U, typename ...SlotArgs>
        Address ImitationFunctionHelper(T *object, U &&t, void(std::decay_t<U>::* func)(SlotArgs...) const, ConnectionType type)
        {
            constexpr bool is_object_v = Implementation::is_object<T,std::thread::id>::value;
            Address address = Address(object, func);
            Handler v_handler;
            v_handler.id = std::this_thread::get_id();
            v_handler.handler = std::make_shared<Implementation::EventHandler<void, std::tuple<SlotArgs...>, Args...>>(std::forward<U>(t));
            v_handler.type = type;
            if constexpr (is_object_v)
            {
                v_handler.id = object->ThreadId();
            }
            AddHandler(address, v_handler);
            return address;
        }

    public:
        Signal()
        {
            m_weakFlag = std::make_shared<WeakFlag>();
        }
        ~Signal()
        {
            m_Handlers.clear();
        }

        std::weak_ptr<WeakFlag> GetWeakFlag()
        {
            return std::weak_ptr<WeakFlag>(m_weakFlag);
        }

        void Emit(const Args &... args)
        {
            std::shared_lock<std::shared_mutex> lock(m_Mutex);
            for (auto &&element: m_Handlers)
            {
                switch (element.second.type)
                {
                    case ConnectionType::AutoConnection:
                    {
                        if (element.second.id == std::this_thread::get_id())
                        {
                            (*element.second.handler)(args...);
                        }
                        else
                        {
                            EventLoop *loop = Implementation::EventLoopManager::GetInstance()->GetEventLoop(element.second.id);
                            if (loop != nullptr)
                            {
                                loop->PostEvent([handler = element.second.handler, args...]
                                {
                                    (*handler)(args...);
                                });
                            }
                        }
                        break;
                    }
                    case ConnectionType::DirectConnection:
                    {
                        (*element.second.handler)(args...);
                        break;
                    }
                    case ConnectionType::QueuedConnection:
                    {
                        EventLoop *loop = Implementation::EventLoopManager::GetInstance()->GetEventLoop(element.second.id);
                        if (loop != nullptr)
                        {
                            loop->PostEvent([handler = element.second.handler, args...]
                            {
                                (*handler)(args...);
                            });
                        }
                        break;
                    }
                    case ConnectionType::BlockingQueuedConnection:
                    {
                        EventLoop *loop = Implementation::EventLoopManager::GetInstance()->GetEventLoop(element.second.id);
                        if (loop != nullptr)
                        {
                            loop->SendEvent([handler = element.second.handler, args...]
                            {
                                (*handler)(args...);
                            });
                        }
                        break;
                    }
                }
            }
        }

    public:
        template<typename Sender, typename Receiver, typename T, typename U, typename ...SignalArgs, typename ...SlotArgs>
        friend constexpr void Connect(Sender *sender, Signal<SignalArgs...> T::* event, Receiver *receiver, void(U::* handler)(SlotArgs...), ConnectionType type);

        template<typename Sender, typename Receiver, typename T, typename U, typename ...SignalArgs, typename ...SlotArgs>
        friend constexpr void Disconnect(Sender *sender, Signal<SignalArgs...> T::* event, Receiver *receiver, void(U::* handler)(SlotArgs...));

        template<typename Sender, typename Receiver, typename T, typename U, typename ...SignalArgs, typename ...SlotArgs>
        friend constexpr void Connect(Sender *sender, Signal<SignalArgs...> T::* event, Receiver *receiver, void(U::* handler)(SlotArgs...) const, ConnectionType type);

        template<typename Sender, typename Receiver, typename T, typename U, typename ...SignalArgs, typename ...SlotArgs>
        friend constexpr void Disconnect(Sender *sender, Signal<SignalArgs...> T::* event, Receiver *receiver, void(U::* handler)(SlotArgs...) const);

        template<typename Sender, typename Receiver, typename T, typename Lambda, typename ...SignalArgs>
        friend constexpr void Connect(Sender *sender, Signal<SignalArgs...> T::* event, Receiver *receiver, Lambda &&lambda, ConnectionType type);

        template<typename Sender, typename T, typename Lambda, typename ...SignalArgs>
        friend constexpr void Connect(Sender* sender, Signal<SignalArgs...> T::* event, Lambda&& lambda);

        template<typename Sender, typename T, typename ...SignalArgs, typename ...SlotArgs>
        friend constexpr void Connect(Sender *sender, Signal<SignalArgs...> T::* event, void(*handler)(SlotArgs...));

        template<typename Sender, typename T, typename ...SignalArgs, typename ...SlotArgs>
        friend constexpr void Disconnect(Sender *sender, Signal<SignalArgs...> T::* event, void(*handler)(SlotArgs...));

    };


    class WeakFlag
    {

    };

    class Object
    {
    private:
        using Address = Implementation::Address;
        using AddressHash = Implementation::AddressHash;
        using ClassFunctionPointer = Implementation::ClassFunctionPointer;
        using Connection = std::map<ClassFunctionPointer, std::function<void()>>;
    private:
        std::atomic<std::thread::id> m_Id;
        std::unordered_map<Address, Connection, AddressHash> m_SendersList;
        std::unordered_map<Address, std::function<void()>, AddressHash> m_ReceiversList;
        mutable std::shared_mutex m_Mutex;
        std::shared_ptr<WeakFlag> m_weakFlag;
    private:
        template<typename Callable>
        void AddSender(const Address &senderAddress, const ClassFunctionPointer &functionAddress, Callable &&func)
        {
            std::unique_lock<std::shared_mutex> lock(m_Mutex);
            m_SendersList[senderAddress].insert(std::make_pair(functionAddress, std::forward<Callable>(func)));
        }

        template<typename Callable>
        void AddReceiver(const Address &receiverAddress, Callable &&func)
        {
            std::unique_lock<std::shared_mutex> lock(m_Mutex);
            m_ReceiversList[receiverAddress] = std::forward<Callable>(func);
        }

        void RemoveSender(const Address &senderAddress, const ClassFunctionPointer &functionAddress)
        {
            std::unique_lock<std::shared_mutex> lock(m_Mutex);
            m_SendersList.at(senderAddress).erase(functionAddress);
        }

        void RemoveReceiver(const Address &address)
        {
            std::unique_lock<std::shared_mutex> lock(m_Mutex);
            m_ReceiversList.erase(address);
        }

        bool ContainSender(const Address &address) const
        {
            std::shared_lock<std::shared_mutex> lock(m_Mutex);
            return m_SendersList.count(address);
        }

        bool ContainReceiver(const Address &address) const
        {
            std::shared_lock<std::shared_mutex> lock(m_Mutex);
            return m_ReceiversList.count(address);
        }

    public:
        template<typename Callable>
        friend void Implementation::AddSender(Object &object, const Address &senderAddress, const ClassFunctionPointer &functionAddress, Callable &&func);

        template<typename Callable>
        friend void Implementation::AddReceiver(Object &object, const Address &receiverAddress, Callable &&func);

        friend void Implementation::RemoveSender(Object &object, const Address &senderAddress, const ClassFunctionPointer &functionAddress);

        friend void Implementation::RemoveReceiver(Object &object, const Address &address);

        friend bool Implementation::ContainSender(const Object &object, const Address &address);

        friend bool Implementation::ContainReceiver(const Object &object, const Address &address);

        Object()
        {
            m_Id = std::this_thread::get_id();
            m_weakFlag = std::make_shared<WeakFlag>();
        }

        virtual ~Object()
        {
            DisconnectAll();
        }

        std::weak_ptr<WeakFlag> GetWeakFlag() noexcept
        {
            return std::weak_ptr<WeakFlag>(m_weakFlag);
        }

        void MoveToThread(const std::thread &target) noexcept
        {
            m_Id = target.get_id();
        }

        void MoveToThread(const std::thread::id &id) noexcept
        {
            m_Id = id;
        }

        void MoveToThread(const Thread &other) noexcept
        {
            m_Id = other.GetID();
        }

        std::thread::id ThreadId() const noexcept
        {
            return m_Id;
        }

        winSignal::EventLoop* GetEventLoop() noexcept
        {
           return winSignal::GetEventLoop(m_Id);
        }

        template<typename Callable>
        void InvokeMethod(Callable&& func, winSignal::ConnectionType type = ConnectionType::AutoConnection)
        {
            switch (type)
            {
            case ConnectionType::AutoConnection:
            {
                if (m_Id == std::this_thread::get_id())
                {
                    func();
                }
                else if (EventLoop* loop = GetEventLoop())
                {
                    loop->PostEvent([=]() {
                        func();
                    });
                }
                break;
            }
            case ConnectionType::DirectConnection:
            {
                func();
                break;
            }
            case ConnectionType::QueuedConnection:
            {
                if (EventLoop* loop = GetEventLoop())
                {
                    loop->PostEvent([=]() {
                        func();
                    });
                }
                break;
            }
            case ConnectionType::BlockingQueuedConnection:
            {
                if (EventLoop* loop = GetEventLoop())
                {
                    loop->SendEvent([=]() {
                        func();
                    });
                    break;
                }
            }
            }
        }

        void DisconnectAll()
        {
            std::unordered_map<Address, Connection, AddressHash> sendersList;
            std::unordered_map<Address, std::function<void()>, AddressHash> receiversList;
            {
                std::shared_lock<std::shared_mutex> lock(m_Mutex);
                sendersList = m_SendersList;
                receiversList = m_ReceiversList;
            }
            for (auto& pair : sendersList)
            {
                for (auto& func : pair.second)
                {
                    func.second();
                }
            }
            for (auto& receiver : receiversList)
            {
                (receiver.second)();
            }
        }

        void DeleteLater()
        {
            DisconnectAll();
            if (EventLoop* loop = GetEventLoop())
            {
                loop->PostEvent([=]() {
                    delete this;
                });
            }
            else
            {
                delete this;
            }
        }

        template<typename Sender, typename Receiver, typename T, typename U, typename ...SignalArgs, typename ...SlotArgs>
        friend constexpr void Connect(Sender *sender, Signal<SignalArgs...> T::* event, Receiver *receiver, void(U::* handler)(SlotArgs...), ConnectionType type);

        template<typename Sender, typename Receiver, typename T, typename U, typename ...SignalArgs, typename ...SlotArgs>
        friend constexpr void Disconnect(Sender *sender, Signal<SignalArgs...> T::* event, Receiver *receiver, void(U::* handler)(SlotArgs...));

        template<typename Sender, typename Receiver, typename T, typename U, typename ...SignalArgs, typename ...SlotArgs>
        friend constexpr void Connect(Sender *sender, Signal<SignalArgs...> T::* event, Receiver *receiver, void(U::* handler)(SlotArgs...) const, ConnectionType type);

        template<typename Sender, typename Receiver, typename T, typename U, typename ...SignalArgs, typename ...SlotArgs>
        friend constexpr void Disconnect(Sender *sender, Signal<SignalArgs...> T::* event, Receiver *receiver, void(U::* handler)(SlotArgs...) const);

        template<typename Sender, typename Receiver, typename T, typename Lambda, typename ...SignalArgs>
        friend constexpr void Connect(Sender *sender, Signal<SignalArgs...> T::* event, Receiver *receiver, Lambda &&lambda, ConnectionType type);

        template<typename Sender, typename T, typename ...SignalArgs, typename ...SlotArgs>
        friend constexpr void Connect(Sender *sender, Signal<SignalArgs...> T::* event, void(*handler)(SlotArgs...));

        template<typename Sender, typename T, typename ...SignalArgs, typename ...SlotArgs>
        friend constexpr void Disconnect(Sender *sender, Signal<SignalArgs...> T::* event, void(*handler)(SlotArgs...));

    };

    class EventLoopObject : public Object
    {
    private:
        winSignal::Thread* m_pThread = nullptr;

    public:
        EventLoopObject()
        {
            m_pThread = new winSignal::Thread([=]()
            {
                winSignal::EventLoop eventLoop;
                eventLoop.Run();
            });
            MoveToThread(*m_pThread);
        }

        virtual ~EventLoopObject()
        {
            if (auto eventLoop = GetEventLoop())
            {
                eventLoop->SendEvent([=](){
                    eventLoop->Quit();
                });
            }

            if (m_pThread)
            {
                delete m_pThread;
                m_pThread = nullptr;
            }
        }
    };

    class Timer : public Object
    {
    public:
        ~Timer()
        {
            Stop();
            m_timerId = 0;
        }

        template<typename Callable>
        static void SingleShot(int interval, Callable&& func)
        {
            auto eventLoop = winSignal::GetEventLoop(std::this_thread::get_id());
            if (eventLoop)
            {
                eventLoop->SetSingleShotTimer(interval, std::forward<Callable>(func));
            }
        }

        template<typename Callable>
        void Start(int interval, Callable&& func)
        {
            if (IsAlive())
                return;

            auto eventLoop = winSignal::GetEventLoop(std::this_thread::get_id());
            if (eventLoop)
            {
                m_timerId = eventLoop->SetRepeatTimer(interval, std::forward<Callable>(func));
            }
        }

        void Start(int interval)
        {
            if (IsAlive())
                return;

            auto eventLoop = winSignal::GetEventLoop(std::this_thread::get_id());
            if (eventLoop)
            {
                m_timerId = eventLoop->SetRepeatTimer(interval, [&]() {
                    timeout.Emit();
                });
            }
        }

        void Stop()
        {
            if (!IsAlive())
                return;

            auto eventLoop = winSignal::GetEventLoop(std::this_thread::get_id());
            if (eventLoop)
            {
                eventLoop->KillTimer(m_timerId);
                m_timerId = 0;
            }
        }

        bool IsAlive()
        {
            return m_timerId > 0;
        }

    public:
        winSignal::Signal<> timeout;

    private:
        UINT_PTR m_timerId = 0;
    };


    template<typename Sender, typename Receiver, typename T, typename U, typename ...SignalArgs, typename ...SlotArgs>
    inline constexpr void Connect(Sender *sender, Signal<SignalArgs...> T::* event, Receiver *receiver, void (U::* handler)(SlotArgs...), ConnectionType type)
    {
        Implementation::Address ReceiverAddress(receiver, handler);
        Implementation::Address SenderAddress(sender, event);

        using Handler = typename Signal<SignalArgs...>::Handler;
        Handler v_handler;
        v_handler.handler = std::make_shared<Implementation::EventHandler<U, std::tuple<SlotArgs...>, SignalArgs...>>((U *) receiver, handler);
        v_handler.type = type;
        v_handler.id = std::this_thread::get_id();

        constexpr bool is_object_v = Implementation::is_object<T, std::thread::id>::value && Implementation::is_object<U, std::thread::id>::value;
        constexpr bool is_not_object_v = !Implementation::is_object<T, std::thread::id>::value && !Implementation::is_object<U, std::thread::id>::value;
        static_assert(is_object_v || is_not_object_v, "Sender and Receiver must both be Object or neither be");
        if constexpr (is_object_v)
        {
            auto receiverWeakFlag = receiver->GetWeakFlag();
            auto senderWeakFlag = sender->GetWeakFlag();
            auto eventWeakFlag = (static_cast<T*>(sender)->*event).GetWeakFlag();
            v_handler.id = receiver->ThreadId();
            sender->AddReceiver(ReceiverAddress, [=]()
            {
                if (!receiverWeakFlag.expired())
                {
                    receiver->RemoveSender(SenderAddress, ReceiverAddress.function);
                }
            });
            receiver->AddSender(SenderAddress, ReceiverAddress.function, [=]()
            {
                if (!eventWeakFlag.expired())
                {
                    (static_cast<T*>(sender)->*event).RemoveHandler(ReceiverAddress);
                }
                if (!senderWeakFlag.expired())
                {
                    sender->RemoveReceiver(ReceiverAddress);
                }
            });
        }
        (static_cast<T *>(sender)->*event).AddHandler(ReceiverAddress, v_handler);
    }

    template<typename Sender, typename Receiver, typename T, typename U, typename ...SignalArgs, typename ...SlotArgs>
    inline constexpr void Disconnect(Sender *sender, Signal<SignalArgs...> T::* event, Receiver *receiver, void(U::* handler)(SlotArgs...))
    {
        Implementation::Address ReceiverAddress(receiver, handler);
        Implementation::Address SenderAddress(sender, event);
        (static_cast<T *>(sender)->*event).RemoveHandler(ReceiverAddress);

        constexpr bool is_object_v = Implementation::is_object<T, std::thread::id>::value && Implementation::is_object<U, std::thread::id>::value;
        constexpr bool is_not_object_v = !Implementation::is_object<T, std::thread::id>::value && !Implementation::is_object<U, std::thread::id>::value;
        static_assert(is_object_v || is_not_object_v, "Sender and Receiver must both be Object or neither be");
        if constexpr (is_object_v)
        {
            if (sender->ContainReceiver(ReceiverAddress) && receiver->ContainSender(SenderAddress))
            {
                sender->RemoveReceiver(ReceiverAddress);
                receiver->RemoveSender(SenderAddress, ReceiverAddress.function);
            }
        }
    }

    template<typename Sender, typename Receiver, typename T, typename U, typename ...SignalArgs, typename ...SlotArgs>
    inline constexpr void Connect(Sender *sender, Signal<SignalArgs...> T::* event, Receiver *receiver, void (U::* handler)(SlotArgs...) const, ConnectionType type)
    {
        Implementation::Address ReceiverAddress(receiver, handler);
        Implementation::Address SenderAddress(sender, event);

        using Handler = typename Signal<SignalArgs...>::Handler;
        Handler v_handler;
        v_handler.handler = std::make_shared<Implementation::EventHandler<U, std::tuple<SlotArgs...>, SignalArgs...>>((U *) receiver, handler);
        v_handler.type = type;
        v_handler.id = std::this_thread::get_id();

        constexpr bool is_object_v = Implementation::is_object<T, std::thread::id>::value && Implementation::is_object<U, std::thread::id>::value;
        constexpr bool is_not_object_v = !Implementation::is_object<T, std::thread::id>::value && !Implementation::is_object<U, std::thread::id>::value;
        static_assert(is_object_v || is_not_object_v, "Sender and Receiver must both be Object or neither be");
        if constexpr (is_object_v)
        {
            auto receiverWeakFlag = receiver->GetWeakFlag();
            auto senderWeakFlag = sender->GetWeakFlag();
            auto eventWeakFlag = (static_cast<T*>(sender)->*event).GetWeakFlag();
            v_handler.id = receiver->ThreadId();
            sender->AddReceiver(ReceiverAddress, [=]()
            {
                 if (!receiverWeakFlag.expired())
                 {
                     receiver->RemoveSender(SenderAddress, ReceiverAddress.function);
                 }
            });
            receiver->AddSender(SenderAddress, ReceiverAddress.function, [=]()
            {
                if (!eventWeakFlag.expired())
                {
                    (static_cast<T*>(sender)->*event).RemoveHandler(ReceiverAddress);
                }
                 if (!senderWeakFlag.expired())
                 {
                     sender->RemoveReceiver(ReceiverAddress);
                 }
            });
        }

        (static_cast<T *>(sender)->*event).AddHandler(ReceiverAddress, v_handler);
    }

    template<typename Sender, typename Receiver, typename T, typename U, typename ...SignalArgs, typename ...SlotArgs>
    inline constexpr void Disconnect(Sender *sender, Signal<SignalArgs...> T::* event, Receiver *receiver, void(U::* handler)(SlotArgs...) const)
    {
        Implementation::Address ReceiverAddress(receiver, handler);
        Implementation::Address SenderAddress(sender, event);
        (static_cast<T *>(sender)->*event).RemoveHandler(ReceiverAddress);

        constexpr bool is_object_v = Implementation::is_object<T, std::thread::id>::value && Implementation::is_object<U, std::thread::id>::value;
        constexpr bool is_not_object_v = !Implementation::is_object<T, std::thread::id>::value && !Implementation::is_object<U, std::thread::id>::value;
        static_assert(is_object_v || is_not_object_v, "Sender and Receiver must both be Object or neither be");
        if constexpr (is_object_v)
        {
            if (sender->ContainReceiver(ReceiverAddress) && receiver->ContainSender(SenderAddress))
            {
                sender->RemoveReceiver(ReceiverAddress);
                receiver->RemoveSender(SenderAddress, ReceiverAddress.function);
            }
        }
    }

    template<typename Sender, typename T, typename ...SignalArgs, typename ...SlotArgs>
    inline constexpr void Connect(Sender *sender, Signal<SignalArgs...> T::* event, void(*handler)(SlotArgs...))
    {
        using Handler = typename Signal<SignalArgs...>::Handler;
        Handler v_handler
        {
            std::this_thread::get_id(),
            std::make_shared<Implementation::EventHandler<void, std::tuple<SlotArgs...>, SignalArgs...>>(handler),
            ConnectionType::DirectConnection,
        };
        (static_cast<T *>(sender)->*event).AddHandler(Implementation::Address(handler), v_handler);
    }

    template<typename Sender, typename T, typename ...SignalArgs, typename ...SlotArgs>
    inline constexpr void Disconnect(Sender *sender, Signal<SignalArgs...> T::* event, void(*handler)(SlotArgs...))
    {
        (static_cast<T *>(sender)->*event).RemoveHandler(Implementation::Address(handler));
    }

    template<typename Sender, typename Receiver, typename T, typename Lambda, typename ...SignalArgs>
    inline constexpr void Connect(Sender *sender, Signal<SignalArgs...> T::* event, Receiver *receiver, Lambda &&lambda, ConnectionType type)
    {
        constexpr bool is_object_v = Implementation::is_object<T, std::thread::id>::value && Implementation::is_object<Receiver, std::thread::id>::value;
        constexpr bool is_not_object_v = !Implementation::is_object<T, std::thread::id>::value && !Implementation::is_object<Receiver, std::thread::id>::value;
        static_assert(is_object_v || is_not_object_v, "Sender and Receiver must both be Object or neither be");
        Implementation::Address SenderAddress(sender, event);
        auto &&ReceiverAddress = (sender->*event).ImitationFunctionHelper(receiver, lambda, &Lambda::operator(), type);
        if constexpr (is_object_v)
        {
            auto receiverWeakFlag = receiver->GetWeakFlag();
            auto senderWeakFlag = sender->GetWeakFlag();
            auto eventWeakFlag = (static_cast<T*>(sender)->*event).GetWeakFlag();
            sender->AddReceiver(ReceiverAddress, [=]()
            {
                if (!receiverWeakFlag.expired())
                {
                    receiver->RemoveSender(SenderAddress, ReceiverAddress.function);
                }
            });
            receiver->AddSender(SenderAddress, ReceiverAddress.function, [=]()
            {
                if (!eventWeakFlag.expired())
                {
                    (static_cast<T*>(sender)->*event).RemoveHandler(ReceiverAddress);
                }
                if (!senderWeakFlag.expired())
                {
                    sender->RemoveReceiver(ReceiverAddress);
                }
            });
        }
    }

    template<typename Sender, typename T, typename Lambda, typename ...SignalArgs>
    inline constexpr void Connect(Sender* sender, Signal<SignalArgs...> T::* event, Lambda&& lambda)
    {
        (sender->*event).ImitationFunctionHelper((void*)nullptr, lambda, &Lambda::operator(), winSignal::ConnectionType::DirectConnection);
    }

    inline EventLoop *GetEventLoop(std::thread::id id)
    {
        return Implementation::EventLoopManager::GetInstance()->GetEventLoop(id);
    }

    inline EventLoop *GetEventLoop()
    {
        return GetEventLoop(std::this_thread::get_id());
    }

    inline EventLoop *GetEventLoop(const Thread &thread)
    {
        return GetEventLoop(thread.GetID());
    }

    inline EventLoop *GetEventLoop(const std::thread &thread)
    {
        return GetEventLoop(thread.get_id());
    }
}

namespace winSignal::Implementation
{
    using FunctionPointer = Implementation::ClassFunctionPointer;

    template<typename Callable>
    inline void AddSender(Object &object, const Address &senderAddress, const FunctionPointer &functionAddress, Callable &&func)
    {
        object.AddSender(senderAddress, functionAddress, std::forward<Callable>(func));
    }

    template<typename Callable>
    inline void AddReceiver(Object &object, const Address &receiverAddress, Callable &&func)
    {
        object.AddReceiver(receiverAddress, std::forward<Callable>(func));
    }

    inline void RemoveSender(Object &object, const Address &senderAddress, const FunctionPointer &functionAddress)
    {
        object.RemoveSender(senderAddress, functionAddress);
    }

    inline void RemoveReceiver(Object &object, const Address &address)
    {
        object.RemoveReceiver(address);
    }

    inline bool ContainSender(const Object &object, const Address &address)
    {
        return object.ContainSender(address);
    }

    inline bool ContainReceiver(const Object &object, const Address &address)
    {
        return object.ContainReceiver(address);
    }
}
#endif //__WIN_SIGNAL_HPP__
