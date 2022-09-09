//*******************************************************************************
#include <list>
#include <mutex>
#include <algorithm>
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

//*******************************************************************************
int main(){
    return 0;
}
//*******************************************************************************
