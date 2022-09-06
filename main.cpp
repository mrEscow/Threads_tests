//*******************************************************************************
#include <iostream>
#include <thread>
#include <QString>
#include <vector>
#include <functional>
#include <numeric>
//*******************************************************************************
void hello1(QString& string){
    std::cout << "FUNC void(QString&) : " << string.toStdString() << std::endl;

    string.clear();

    string = "STD::REF(T)";
}
//*******************************************************************************
void hello2(QString&& string){
    std::cout << "FUNC void(QString&&) : " << string.toStdString() << std::endl;

    string.clear();

    string = "STD::MOVE(T)";
}
//*******************************************************************************
class Big{
public:
    void hi(const QString& string){
        std::cout << "CLASS BIG::FUNC_HI()" << string.toStdString() << std::endl;
    }
};
//*******************************************************************************
class thread_guard
{
    std::thread& t;
public:
    explicit thread_guard(std::thread& t_) : t(t_)
    {}
    ~thread_guard()
    {
        if(t.joinable())
            {
                t.join();
            }
    }
    thread_guard(thread_guard const&)=delete;
    thread_guard& operator=(thread_guard const&)=delete;
};
//*******************************************************************************
class scoped_thread
{
    std::thread t;
public:
    explicit scoped_thread(std::thread t_) : t(std::move(t_))
    {
    if(!t.joinable())
        throw std::logic_error("No thread");
    }
    ~scoped_thread()
    {
        t.join();
    }
    scoped_thread(scoped_thread const&)=delete;
    scoped_thread& operator=(scoped_thread const&)=delete;
};
//*******************************************************************************
void do_work(unsigned id){std::cout << "MEGA_WORK : " << id << std::endl; }
void f()
{
    std::vector<std::thread> threads;

    for(unsigned i=0;i<20;++i)
        threads.push_back(std::thread(do_work,i)); // Запуск потоков

    // Поочередный вызов join() для каждого потока
    std::for_each(threads.begin(),threads.end(), std::mem_fn(&std::thread::join));
}
//*******************************************************************************
template<typename Iterator,typename T>
struct accumulate_block
{
    void operator()(Iterator first, Iterator last,T& result)
    {
        result = std::accumulate(first, last, result);
    }
};
template<typename Iterator,typename T>
T parallel_accumulate(Iterator first,Iterator last,T init)
{
    unsigned long const length = std::distance(first,last);
    if(!length)
        return init;

    unsigned long const min_per_thread = 25;
    unsigned long const max_threads = (length + min_per_thread - 1) / min_per_thread;
    unsigned long const hardware_threads = std::thread::hardware_concurrency();
    unsigned long const num_threads = std::min(hardware_threads != 0 ? hardware_threads : 2 , max_threads);
    unsigned long const block_size = length / num_threads;
    std::vector<T> results(num_threads);
    std::vector<std::thread> threads(num_threads - 1);
    Iterator block_start = first;
    for(unsigned long i = 0; i < (num_threads - 1); ++i)
    {
        Iterator block_end = block_start;
        std::advance(block_end, block_size);
        threads[i] = std::thread(accumulate_block<Iterator,T>(), block_start, block_end, std::ref(results[i]));
        block_start = block_end;
    }
    accumulate_block<Iterator,T>()(block_start, last, results[num_threads - 1]);
    std::for_each(threads.begin(), threads.end(), std::mem_fn(&std::thread::join));
    return std::accumulate(results.begin(),results.end(),init);
}
//*******************************************************************************
int main(){
    // Базовые операции управления потоками
    // join(), detach(), joinable(), hardware_concurrency(), get_id()

    // Передача аргументов функции потока
    QString string = "Hello Parallel World!";
    std::thread thread(hello1, std::ref(string));
    std::cout << "MAIN: " << string.toStdString() << std::endl;
    if(thread.joinable())
        thread.join();
    std::cout << "MAIN: " << string.toStdString() << std::endl;

    Big big;
    QString string2 = "TEXT";
    std::thread thread2(&Big::hi, &big, string2);
    if(thread2.joinable())
        thread2.join();

    QString string3 = "Hello World!";
    std::thread thread3(hello2, std::move(string3));
    if(thread3.joinable())
        thread3.join();
    std::cout << "MAIN: " << string3.toStdString() << std::endl;

    // Передача владения потоком
    QString string4 = "Hello Move Parallel World!";
    std::thread thread4(hello1, std::ref(string4));
    std::thread thread_Move = std::move(thread4);
    if(thread_Move.joinable())
        thread_Move.join();
    if(thread4.joinable())
        thread4.join();

    // Запуск нескольких потоков и ожидание их завершения
    f();

    // Задание количества потоков во время выполнения
    std::cout << "Count theads: "<< std::thread::hardware_concurrency() << std::endl;
    std::vector<int> intVec;
    intVec.push_back(5);
    intVec.push_back(5);
    intVec.push_back(5);
    intVec.push_back(5);
    intVec.push_back(5);
    int result = 0;
    int resultFromPA = parallel_accumulate(intVec.begin(),intVec.end(),result);
    std::cout << result << std::endl;
    std::cout << resultFromPA << std::endl;

    //  Идентификация потоков
    std::cout<<std::this_thread::get_id() << std::endl;

    return 0;
}
//*******************************************************************************
