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
int main(){

    return 0;
}
//*******************************************************************************
