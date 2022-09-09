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
int main(){
    return 0;
}
//*******************************************************************************
