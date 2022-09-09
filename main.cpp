//*******************************************************************************
#include <list>
#include <memory>
#include <mutex>
#include <algorithm>
#include <exception>
#include <stack>
//*******************************************************************************

//  Защита списка с помощью мьютекса
std::list<int> some_list;
std::mutex some_mutex;
void add_to_list(int new_value){
    std::lock_guard<std::mutex> guard(some_mutex);
    some_list.push_back(new_value);
}
bool list_contains(int value_to_find){
    std::lock_guard<std::mutex> guard(some_mutex);
    return std::find(some_list.begin(), some_list.end(), value_to_find) != some_list.end();
}
//*******************************************************************************

// Непреднамеренная передача наружу ссылки на защищенные данные
class some_data
{
    int a;
    std::string b;
public:
    void do_something();
};
class data_wrapper
{
private:
    some_data data;
    std::mutex m;
public:
    template<typename Function>
    void process_data(Function func)
    {
        std::lock_guard<std::mutex> l(m);
        func(data); // Передаем "защищенные" данные пользовательской функции
     }
};
some_data* unprotected;
void malicious_function(some_data& protected_data)
{
    unprotected = &protected_data;
}
data_wrapper x;
void foo()
{
    x.process_data(malicious_function); // Передаем вредоносную функцию
    unprotected->do_something(); // Доступ к "защищенным" данным в обход защиты
}
/*
   Не передавайте указатели и ссылки на защищенные
   данные за пределы области видимости блокировки никаким способом,
   будь то возврат из функции, сохранение в видимой извне памяти или
   передача в виде аргумента пользовательской функции.
                                                                    */
//*******************************************************************************

// Выявление состояний гонки, внутренне присущих интерфейсам

// Пример определения потокобезопасного стека:
struct empty_stack: std::exception
{
    const char* what() const throw();
};
// -----
template<typename T>
class threadsafe_stack{
public:
    threadsafe_stack();
    threadsafe_stack(const threadsafe_stack&);
    threadsafe_stack& operator=(const threadsafe_stack&) = delete;
    void push(T new_value);
    std::shared_ptr<T> pop(); //  возбуждают исключение empty_stack
    void pop(T& value); //  возбуждают исключение empty_stack
    bool empty() const;
};

// Определение класса потокобезопасного стека
template<typename T>
class threadsafe_stack2
{
private:
    std::stack<T> data;
    mutable std::mutex m;
public:
    threadsafe_stack2(){}
    threadsafe_stack2(const threadsafe_stack2& other)
    {
        std::lock_guard<std::mutex> lock(other.m);
        data=other.data; // Копирование производится в теле конструктора
    }
        threadsafe_stack2& operator=(const threadsafe_stack2&) = delete;
    void push(T new_value)
    {
        std::lock_guard<std::mutex> lock(m);
        data.push(new_value);
    }
    std::shared_ptr<T> pop() // Перед тем как выталкивать значение, проверяем, не пуст ли стек
    {
        std::lock_guard<std::mutex> lock(m);
        if(data.empty()) throw empty_stack();
        std::shared_ptr<T> const res(std::make_shared<T>(data.top()));
        data.pop(); // Перед тем как модифицировать стек в функции pop(), выделяем память для возвращаемого значения
        return res;
    }
    void pop(T& value)
    {
        std::lock_guard<std::mutex> lock(m);
        if(data.empty()) throw empty_stack();
        value=data.top();
        data.pop();
    }
    bool empty() const
    {
        std::lock_guard<std::mutex> lock(m);
        return data.empty();
    }
};
//*******************************************************************************
// Взаимоблокировка: проблема и решение
//*******************************************************************************
//  Применение std::lock() и std::lock_guard для реализации операции обмена
class some_big_object{};
void swap(some_big_object& lhs,some_big_object& rhs);
class X
{
private:
    some_big_object some_detail;
    std::mutex m;
public:
    X(some_big_object const& sd) : some_detail(sd){}
    friend void swap(X& lhs, X& rhs)
    {
        if(&lhs == &rhs)
            return;
        std::lock(lhs.m, rhs.m);
        std::lock_guard<std::mutex> lock_a(lhs.m, std::adopt_lock);
        std::lock_guard<std::mutex> lock_b(rhs.m, std::adopt_lock);
        swap(lhs.some_detail, rhs.some_detail);
    }
};
/*
    Хотя std::lock помогает избежать взаимоблокировки в случаях,
        когда нужно захватить сразу два или более мьютексов , она не в си-
        лах помочь
                    */

//*******************************************************************************
//  Использование иерархии блокировок для предотвращения взаимоблокировки
class hierarchical_mutex
{
    std::mutex internal_mutex;
    unsigned long const hierarchy_value;
    unsigned previous_hierarchy_value;
    static thread_local unsigned long this_thread_hierarchy_value;
    void check_for_hierarchy_violation()
    {
        if(this_thread_hierarchy_value <= hierarchy_value)
        {
            throw std::logic_error("mutex hierarchy violated");
        }
    }
    void update_hierarchy_value()
    {
        previous_hierarchy_value = this_thread_hierarchy_value;
        this_thread_hierarchy_value = hierarchy_value;
    }
public:
    explicit hierarchical_mutex(unsigned long value):
    hierarchy_value(value),
    previous_hierarchy_value(0)
    {}
    void lock()
    {
        check_for_hierarchy_violation();
        internal_mutex.lock();
        update_hierarchy_value();
    }
    void unlock()
    {
        this_thread_hierarchy_value = previous_hierarchy_value;
        internal_mutex.unlock();
    }
    bool try_lock()
    {
        // Защита разделяемых данных с помощью мьютексов
        // Разделение данных между потоками
        check_for_hierarchy_violation();
        if(!internal_mutex.try_lock())
            return false;
        update_hierarchy_value();
        return true;
    }
};
thread_local unsigned long
hierarchical_mutex::this_thread_hierarchy_value(ULONG_MAX);
//------
hierarchical_mutex high_level_mutex(10000);
hierarchical_mutex low_level_mutex(5000);

int do_low_level_stuff();

int low_level_func()
{
    std::lock_guard<hierarchical_mutex> lk(low_level_mutex);
    return do_low_level_stuff();
}
void high_level_stuff(int some_param);
void high_level_func()
{
    std::lock_guard<hierarchical_mutex> lk(high_level_mutex);
    high_level_stuff(low_level_func());
}
void thread_a()
{
    high_level_func();
}
hierarchical_mutex other_mutex(100);
void do_other_stuff();
void other_stuff()
{
    high_level_func();
    do_other_stuff();
}
void thread_b()
{
    // Защита разделяемых данных с помощью мьютексов
    // Разделение данных между потоками
    std::lock_guard<hierarchical_mutex> lk(other_mutex);
    other_stuff();
}
//*******************************************************************************
int main(){

    return 0;
}
//*******************************************************************************
