#include <iostream>
#include "../winsignal.hpp"
#include <string>
#include <sstream>

template<typename... Args>
void Print(Args&&... args) {
    ((std::cout << std::forward<Args>(args) << " "), ...);
}

class Window : public winSignal::Object
{
public:
    Window()
    {
    }
    ~Window()
    {
        Print(" ~Window() thread id:", std::this_thread::get_id(), "\n");
    }
    winSignal::Signal<int, char, std::string> event;
};

class Button : public winSignal::EventLoopObject
{
private:
    winSignal::Timer timer;
    int tickcount = 0;
public:
    Button()
    {
        Print("Button()", " thread id: ", std::this_thread::get_id(), "\n");
        winSignal::Connect(&timer, &winSignal::Timer::timeout, this, [=]() {
            Print("repeat timer tick ------------" , tickcount++ , " thread id:" , std::this_thread::get_id() , "\n");
            if (tickcount > 15)
            {
                timer.Stop();
            }
            else if (tickcount % 5 == 0)
            {
                singleSlotTest();
            }
        });
    }

    ~Button()
    {
        Print("~Button()", " thread id: ", std::this_thread::get_id(), "\n");
    }

    void singleSlotTest()
    {
        winSignal::Timer::SingleShot(1000, []() {
            Print("singleSlot timer tick ------------", std::this_thread::get_id(), "\n") ;
        });
    }

    void test()
    {
        InvokeMethod([=]() {
            Print("InvokeMethod thread id is :" , std::this_thread::get_id() , "\n");

            singleSlotTest();
            timer.Start(1000);

        });

        //timer.Start(1000, [=]() {
        //    Print("this is repeat timer, thread id:" , std::this_thread::get_id() , "\n");
        //}, ThisObject());
    }

    void OnClick(int a, char b)
    {
        Print("Button OnClick", a, " ", b, " ", " thread id: ", std::this_thread::get_id(), "\n");
    }
    void Show(int a) const
    {
        Print("Button Show", a, " thread id: ", std::this_thread::get_id(), "\n");
    }
};

class Label : public winSignal::Object
{
public:
    ~Label()
    {
        Print(" ~Label", std::this_thread::get_id(), "\n");
    }
    void TextChanged(std::string text)
    {
        Print("Label TextChanged", text, " thread id: ", std::this_thread::get_id(), "\n");
    }
};

static void test()
{
    Print("static test ", " thread id: ", std::this_thread::get_id(), "\n");
}

class A
{
public:
    winSignal::Signal<int, char, std::string> event;
};

class B
{
public:
    void onSlot(int a, char c, std::string s)
    {
        Print("B onSlot", a, " ", c, s, " thread id: ", std::this_thread::get_id(), "\n");
    }
};

int main()
{
    Print("Main ", " thread id: ", std::this_thread::get_id(), "\n");
    Window* window = new Window();
    Button* button = new Button();
    Label* label = new Label();
    //std::this_thread::sleep_for(std::chrono::seconds(1));

    winSignal::Connect(window, &Window::event, test);
    winSignal::Connect(window, &Window::event, button, &Button::OnClick);
    winSignal::Connect(window, &Window::event, label, &Label::TextChanged);
    winSignal::Connect(window, &Window::event, [=](int a, char c, std::string d)
        {
            Print("Button Lambda", a, " ", c, " thread id: ", std::this_thread::get_id(), "\n");
        });
    std::cout << "------------------------\n";
    button->test();
    window->event.Emit(1, 'a', "hello");
    //winSignal::Disconnect(window, &Window::event, button, &Button::OnClick);

    A* a = new A();
    B* b = new B();

    winSignal::Connect(a, &A::event, []() {
        Print("Button Lambdaaaaaaaaaaaaaaaaaadddddddddddddddddddddd", " ", " thread id: ", std::this_thread::get_id(), "\n");
    });
    winSignal::Connect(a, &A::event, b, &B::onSlot);
    a->event.Emit(1, 'c', "222");

    std::this_thread::sleep_for(std::chrono::seconds(12));
    button->DeleteLater();
    label->DeleteLater();
    window->DeleteLater();
    std::this_thread::sleep_for(std::chrono::seconds(2));
    system("pause");
    return 0;
}
