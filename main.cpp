//*******************************************************************************
#include <list>
#include <memory>
#include <mutex>
#include <algorithm>
#include <exception>
#include <stack>
#include <queue>
#include <condition_variable>
//*******************************************************************************
// Синхронизация параллельных операций
//*******************************************************************************
// Ожидание данных с помощью std::condition_variable
class data_chunk{};
bool more_data_to_prepare();
data_chunk prepare_data();
void process(data_chunk);
bool is_last_chunk(data_chunk);
//------
std::mutex mut;
std::queue<data_chunk> data_queue;
std::condition_variable data_cond;
void data_preparation_thread()
{
    while(more_data_to_prepare())
    {
        data_chunk const data = prepare_data();
        std::lock_guard<std::mutex> lk(mut);
        data_queue.push(data);
        data_cond.notify_one();
    }
}
void data_processing_thread()
{
    while(true)
    {
        std::unique_lock<std::mutex> lk(mut);
        data_cond.wait(lk, []{
            return !data_queue.empty();
        });
        data_chunk data = data_queue.front();
        data_queue.pop();
        lk.unlock();
        process(data);
        if(is_last_chunk(data))
            break;
    }
}
//*******************************************************************************
// Полное определение класса потокобезопасной очереди на базе условных переменных
//#include <queue>
//#include <memory>
//#include <mutex>
template<typename T>
class threadsafe_queue
{
private:
    mutable std::mutex mut; // Мьютекс должен быть изменяемым
    std::queue<T> data_queue;
    std::condition_variable data_cond;
public:
    threadsafe_queue()
    {}
    threadsafe_queue(threadsafe_queue const& other)
    {
        std::lock_guard<std::mutex> lk(other.mut);
        data_queue=other.data_queue;
    }
    void push(T new_value)
    {
        std::lock_guard<std::mutex> lk(mut);
        data_queue.push(new_value);
        data_cond.notify_one();
    }
    void wait_and_pop(T& value)
    {
        std::unique_lock<std::mutex> lk(mut);
        data_cond.wait(lk,[this]{return !data_queue.empty();});
        value=data_queue.front();
        data_queue.pop();
    }
    std::shared_ptr<T> wait_and_pop()
    {
        std::unique_lock<std::mutex> lk(mut);
        data_cond.wait(lk,[this]{return !data_queue.empty();});
        std::shared_ptr<T> res(std::make_shared<T>(data_queue.front()));
        data_queue.pop();
        return res;
    }
    bool try_pop(T& value)
    {
        std::lock_guard<std::mutex> lk(mut);
        if(data_queue.empty())
            return false;
        value=data_queue.front();
        data_queue.pop();
        return true;
    }
    std::shared_ptr<T> try_pop()
    {
        std::lock_guard<std::mutex> lk(mut);
        if(data_queue.empty())
            return std::shared_ptr<T>();
        std::shared_ptr<T> res(std::make_shared<T>(data_queue.front()));
        data_queue.pop();
        return res;
    }
    bool empty() const
    {
        std::lock_guard<std::mutex> lk(mut);
        return data_queue.empty();
    }
};
//*******************************************************************************
//  Использование std::future для получения результата асинхронной задачи
#include <future>
#include <iostream>
int find_the_answer_to_ltuae();
void do_other_stuff();
void main_()
{
    std::future<int> the_answer = std::async(find_the_answer_to_ltuae);
    do_other_stuff();
    std::cout << "Ответ равен "<< the_answer.get() << std::endl;
}
//*******************************************************************************
// Передача аргументов функции, заданной в std::async
#include <string>
#include <future>

struct X
{
 void foo(int,std::string const&);
 std::string bar(std::string const&);
};

X x;
auto f1=std::async(&X::foo, &x, 42, "hello"); // Вызывается p->foo(42,"hello"), где p=&x
auto f2=std::async(&X::bar, x, "goodbye"); // Вызывается tmpx.bar("goodbye"), где tmpx – копия x
struct Y
{
 double operator()(double);
};

Y y;
auto f3=std::async(Y(),3.141); // Вызывается tmpy(3.141), где tmpy создается из Y перемещающим конструктором
auto f4=std::async(std::ref(y),2.718); // Вызывается y(2.718)

X baz(X&);
//std::async(baz, std::ref(x)); // Вызывается baz(x)

class move_only
{
public:
    move_only();
    move_only(move_only&&);
    move_only(move_only const&) = delete;
    move_only& operator=(move_only&&);
    move_only& operator=(move_only const&) = delete;
    void operator()();
};
auto f5 = std::async(move_only());
//*******************************************************************************
// Определение частичной специализации std::packaged_task<>
template<>
class std::packaged_task<std::string(std::vector<char>*,int)>
{
public:
    template<typename Callable>
    explicit packaged_task(Callable&& f);
    std::future<std::string> get_future();
    void operator()(std::vector<char>*,int);
};
//*******************************************************************************
// Передача задач между потоками:
//*******************************************************************************
// Выполнение кода в потоке пользовательского интерфейса с применением std::packaged_task
#include <deque>
#include <mutex>
#include <future>
#include <thread>
#include <utility>
std::mutex m;
std::deque<std::packaged_task<void()> > tasks;
bool gui_shutdown_message_received();
void get_and_process_gui_message();
void gui_thread()
{
    while(!gui_shutdown_message_received())
        {
            get_and_process_gui_message();
            std::packaged_task<void()> task;
            {
                std::lock_guard<std::mutex> lk(m);
                if(tasks.empty())
                    continue;
                task = std::move(tasks.front());
                tasks.pop_front();
            }
            task();
        }
}
std::thread gui_bg_thread(gui_thread);
template<typename Func>
std::future<void> post_task_for_gui_thread(Func f)
{
    std::packaged_task<void()> task(f);
    std::future<void> res = task.get_future();
    std::lock_guard<std::mutex> lk(m);
    tasks.push_back(std::move(task));
    return res;
}
//*******************************************************************************
// Обработка нескольких соединений в одном потоке с помощью объектов-обещаний
//    #include <future>
//    void process_connections(connection_set& connections)
//    {
//        while(!done(connections))
//            {
//                for(connection_iterator connection = connections.begin(), end = connections.end();
//                 connection! = end;
//                 ++connection;)
//                {
//                    if(connection->has_incoming_data())
//                    {
//                            data_packet data=connection->incoming();
//                            std::promise<payload_type>& p = connection->get_promise(data.id);
//                            p.set_value(data.payload);
//                    }
//                    if(connection->has_outgoing_data())
//                    {
//                            outgoing_packet data = connection->top_of_outgoing_queue();
//                            connection->send(data.payload);
//                            data.promise.set_value(true);
//                    }
//                }
//            }
//    }
//*******************************************************************************
//  Ожидание условной переменной с таймаутом
#include <condition_variable>
#include <mutex>
#include <chrono>
std::condition_variable cv;
bool done;
std::mutex mutex;
bool wait_loop()
{
    auto const timeout= std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    std::unique_lock<std::mutex> lk(mutex);
    while(!done)
    {
        if(cv.wait_until(lk,timeout) == std::cv_status::timeout)
            break;
    }
    return done;
}
//*******************************************************************************
// Параллельная реализация Quicksort с применением будущих результатов
template<typename T>
std::list<T> parallel_quick_sort(std::list<T> input)
{
    if(input.empty())
    {
        return input;
    }
    std::list<T> result;
    result.splice(result.begin(), input, input.begin());
    T const& pivot =* result.begin();
    auto divide_point=std::partition(input.begin(), input.end(), [&](T const& t){
        return t<pivot;
    });
    std::list<T> lower_part;
    lower_part.splice(lower_part.end(),input,input.begin(), divide_point);
    std::future<std::list<T> > new_lower(std::async(&parallel_quick_sort<T> ,std::move(lower_part)));
    auto new_higher(parallel_quick_sort(std::move(input)));
    result.splice(result.end(), new_higher);
    result.splice(result.begin(), new_lower.get());
    return result;
}
//*******************************************************************************
//  Простая реализация функции spawn_task
template<typename F,typename A>
std::future<typename std::result_of<F(A&&)>::type>
spawn_task(F&& f,A&& a)
{
     typedef typename std::result_of<F(A&&)>::type result_type;
     std::packaged_task<result_type(A&&)>
     task(std::move(f));
     std::future<result_type> res(task.get_future());
     std::thread t(std::move(task),std::move(a));
     t.detach();
     return res;
}
//*******************************************************************************
// Простая реализация класса, описывающего логику работы банкомата
struct card_inserted
{
    std::string account;
};
//class atm
//{
//    messaging::receiver incoming;
//    messaging::sender bank;
//    messaging::sender interface_hardware;
//    void (atm::*state)();
//    std::string account;
//    std::string pin;
//    void waiting_for_card()
//    {
//        interface_hardware.send(display_enter_card());
//            incoming.wait()

//                .handle<card_inserted>(
//                    [&](card_inserted const& msg)
//                    {
//                        account=msg.account;
//                        pin="";
//                             interface_hardware.send(display_enter_pin());
//                        state=&atm::getting_pin;
//                    }
//                    );
//    }
//    void getting_pin();
//public:
//    void run()
//    {
//        state = &atm::waiting_for_card;
//        try
//        {
//            for(;;)
//            {
//                (this->*state)();
//            }
//        }
//        catch(messaging::close_queue const&)
//        {
//        }
//    }
//};
//*******************************************************************************
int main(){

    return 0;
}
//*******************************************************************************
