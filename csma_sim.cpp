// csma_sim.cpp
// Компилировать с: g++ -std=c++17 csma_sim.cpp -O2 -o csma_sim
// Пример запуска:
// ./csma_sim --N 50 --alpha 50 --simtime 10 --B 100 --L 30 --l 1500 --qlimit -1 --csv results.csv

// #include <bits/stdc++.h>
#include <iostream>
#include <vector>
#include <deque>
#include <queue>
#include <string>
#include <random>
#include <fstream>
#include <iomanip>
#include <cstdint>
using namespace std;

// Структура пакета
struct Packet {
    uint64_t id;              // уникальный ID пакета
    int host;                 // хост-отправитель
    double arrival_time;      // время прибытия в очередь
    int attempts;             // количество попыток передачи
    Packet(uint64_t _id=0,int _host=0,double _arrival_time=0.0,int _attempts=0)
        : id(_id), host(_host), arrival_time(_arrival_time), attempts(_attempts) {}
};

// Типы событий в симуляции
enum EventType { EVT_ARRIVAL, EVT_TX_END };

// Структура события
struct Event {
    double time;              // время события
    EventType type;           // тип события (прибытие или конец передачи)
    int host;                 // номер хоста
    uint64_t pkt_id;          // ID пакета
    Event(double t=0.0, EventType et=EVT_ARRIVAL, int h=0, uint64_t pid=0)
        : time(t), type(et), host(h), pkt_id(pid) {}
};

// Компаратор для приоритетной очереди (минимальное время первым)
// ВАЖНО: priority_queue в C++ - это max-heap по умолчанию, поэтому используем >
// чтобы получить min-heap (события с меньшим временем будут на вершине)
// Если времена равны, то EVT_ARRIVAL (0) идет раньше EVT_TX_END (1)
struct EventComp {
    bool operator()(Event const& a, Event const& b) const {
        if (a.time != b.time) return a.time > b.time;  // сравниваем по времени (инвертировано для min-heap)
        return a.type > b.type;                         // если времена равны - по типу события
    }
};

int main(int argc, char* argv[]) {
    int N = 10;
    double alpha = 10.0;
    double simTime = 10.0;
    double B = 100.0;
    double L = 3.0;
    int l_bytes = 1500;
    int qlimit = -1;
    string csv_file = "";

    for (int i=1;i<argc;i++){
        string s = argv[i];
        if (s=="--N" && i+1<argc) N = stoi(argv[++i]);
        else if (s=="--alpha" && i+1<argc) alpha = stod(argv[++i]);
        else if (s=="--simtime" && i+1<argc) simTime = stod(argv[++i]);
        else if (s=="--B" && i+1<argc) B = stod(argv[++i]);
        else if (s=="--L" && i+1<argc) L = stod(argv[++i]);
        else if (s=="--l" && i+1<argc) l_bytes = stoi(argv[++i]);
        else if (s=="--qlimit" && i+1<argc) qlimit = stoi(argv[++i]);
        else if (s=="--csv" && i+1<argc) csv_file = argv[++i];
        else {
            cerr<<"Unknown or malformed arg: "<<s<<"\n";
            return 1;
        }
    }

    // Вычисление времени передачи и задержки распространения
    // tx_time = (размер в битах) / (пропускная способность в бит/сек)
    // l_bytes * 8 = размер пакета в битах
    // B * 1e6 = пропускная способность в бит/сек (B задана в Мбит/сек)
    const double tx_time = (double)l_bytes * 8.0 / (B * 1e6); // время передачи пакета (сек)
    
    // prop_delay = расстояние / скорость света в кабеле
    // 2e8 м/с ≈ скорость света в медном кабеле (примерно 2/3 от скорости света в вакууме)
    const double prop_delay = L / 2e8; // задержка распространения (сек)

    // Инициализация генератора случайных чисел
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::exponential_distribution<double> exp_dist(alpha);

    // Приоритетная очередь событий (отсортирована по времени)
    priority_queue<Event, vector<Event>, EventComp> eq;

    // Очереди пакетов для каждого хоста
    vector< deque<Packet> > queues(N);

    // Статистика по очередям
    vector<double> last_queue_change_time(N, 0.0);
    vector<double> queue_time_area(N, 0.0);      // интеграл длины очереди по времени
    vector<int> max_queue_len(N, 0);             // максимальная длина очереди

    // Счетчики статистики
    uint64_t global_pkt_id = 0;
    uint64_t total_created = 0;
    uint64_t total_dropped = 0;
    uint64_t total_transmitted = 0;
    uint64_t total_attempts_sum = 0;

    // Состояние канала
    bool channel_busy = false;
    double channel_busy_start = 0.0;
    double cumulative_channel_busy = 0.0;       // общее время занятости канала
    int current_tx_host = -1;
    uint64_t current_tx_pktid = 0;
    int current_tx_attempts = 0;

    // Инициализация первых событий прибытия для каждого хоста
    for (int i=0;i<N;i++){
        double t = exp_dist(gen);
        eq.push(Event(t, EVT_ARRIVAL, i, 0));
    }

    double now = 0.0;

    // === ОСНОВНОЙ ЦИКЛ СИМУЛЯЦИИ ===
    while (!eq.empty()) {
        Event ev = eq.top(); eq.pop();
        if (ev.time > simTime) break;

        now = ev.time;

        // Обновление статистики очередей
        // Используем метод трапеций: интегрируем длину очереди по времени
        // queue_time_area = сумма (длина_очереди * время_между_событиями)
        // Это нужно для вычисления средней длины очереди: avg = queue_time_area / simTime
        for (int i=0;i<N;i++){
            double dt = now - last_queue_change_time[i];
            if (dt > 0.0) {
                queue_time_area[i] += queues[i].size() * dt;  // добавляем площадь прямоугольника
                last_queue_change_time[i] = now;              // обновляем время последнего события
            }
        }

        // === СОБЫТИЕ: ПРИБЫТИЕ ПАКЕТА ===
        if (ev.type == EVT_ARRIVAL) {
            int h = ev.host;
            uint64_t pid = ++global_pkt_id;
            total_created++;

            // Проверка: есть ли пакеты в очередях других хостов
            // Это нужно для справедливого распределения канала между хостами
            bool somebody_waiting = false;
            for (int j=0;j<N;j++) {
                if (!queues[j].empty()) { somebody_waiting = true; break; }
            }

            // Если канал свободен И нет ожидающих пакетов И своя очередь пуста
            // то передаем сразу (без очереди). Это оптимизация для минимизации задержки
            if (!channel_busy && !somebody_waiting && queues[h].empty()) {
                channel_busy = true;
                channel_busy_start = now;
                current_tx_host = h;
                current_tx_pktid = pid;
                current_tx_attempts = 1;

                eq.push(Event(now + tx_time + prop_delay, EVT_TX_END, h, pid));
            } else {
                // Иначе добавляем в очередь или отбрасываем
                if (qlimit >= 0 && (int)queues[h].size() >= qlimit) {
                    total_dropped++;
                } else {
                    Packet p(pid, h, now, 0);
                    queues[h].push_back(p);
                    if ((int)queues[h].size() > max_queue_len[h]) max_queue_len[h] = (int)queues[h].size();
                }
            }

            // Генерируем следующее событие прибытия для этого хоста
            double next_t = now + exp_dist(gen);
            if (next_t <= simTime) eq.push(Event(next_t, EVT_ARRIVAL, h, 0));
        }
        // === СОБЫТИЕ: КОНЕЦ ПЕРЕДАЧИ ===
        else if (ev.type == EVT_TX_END) {

            // Обновляем время занятости канала
            if (channel_busy) {
                cumulative_channel_busy += (now - channel_busy_start);
                channel_busy = false;
            }

            // Обновляем статистику
            total_transmitted++;
            total_attempts_sum += (uint64_t)max(1, current_tx_attempts);

            // Очищаем информацию о текущей передаче
            current_tx_host = -1;
            current_tx_pktid = 0;
            current_tx_attempts = 0;

            // Ищем пакет с наименьшим временем прибытия (FIFO по очередям)
            double best_arrival = 1e300;
            int best_host = -1;
            for (int i=0;i<N;i++){
                if (!queues[i].empty()) {
                    double ta = queues[i].front().arrival_time;
                    if (ta < best_arrival) { best_arrival = ta; best_host = i; }
                }
            }

            // Если есть пакеты в очередях - передаем следующий
            if (best_host != -1) {
                Packet p = queues[best_host].front();
                queues[best_host].pop_front();

                p.attempts = 1;

                channel_busy = true;
                channel_busy_start = now;
                current_tx_host = best_host;
                current_tx_pktid = p.id;
                current_tx_attempts = p.attempts;

                eq.push(Event(now + tx_time + prop_delay, EVT_TX_END, best_host, p.id));
            }
        }
    }


    // === ФИНАЛИЗАЦИЯ СТАТИСТИКИ ===
    // Добавляем оставшееся время в конце симуляции
    for (int i=0;i<N;i++){
        double dt = simTime - last_queue_change_time[i];
        if (dt > 0.0) queue_time_area[i] += queues[i].size() * dt;
    }
    if (channel_busy) {
        cumulative_channel_busy += (simTime - channel_busy_start);
    }

    // === ВЫЧИСЛЕНИЕ МЕТРИК ===
    double lost_fraction = (total_created>0) ? ((double)total_dropped / (double)total_created) : 0.0;
    double avg_attempts = (total_transmitted>0) ? ((double)total_attempts_sum / (double)total_transmitted) : 0.0;

    // Средняя длина очереди для каждого хоста
    vector<double> avg_queue_len(N);
    double sum_avg_queue_len = 0.0;
    int max_queue_overall = 0;
    for (int i=0;i<N;i++){
        avg_queue_len[i] = queue_time_area[i] / simTime;
        sum_avg_queue_len += avg_queue_len[i];
        if (max_queue_len[i] > max_queue_overall) max_queue_overall = max_queue_len[i];
    }
    double mean_avg_queue_len = sum_avg_queue_len / (N>0 ? N : 1);
    double channel_util = cumulative_channel_busy / simTime;  // утилизация канала

    cout.setf(std::ios::fixed); cout<<setprecision(6);
    cout<<"Simulation results (CSMA model)\n";
    cout<<"Parameters:\n";
    cout<<"  N="<<N<<" alpha="<<alpha<<" mean_interval="<<(1.0/alpha)<<"s simTime="<<simTime<<"s\n";
    cout<<"  B="<<B<<" Mbps, packet l="<<l_bytes<<" bytes, tx_time="<<tx_time
        <<" s, prop_delay="<<prop_delay<<" s\n";
    if (qlimit<0) cout<<"  qlimit=unlimited\n"; else cout<<"  qlimit="<<qlimit<<"\n";
    cout<<"\nResults:\n";
    cout<<"  total_created = "<<total_created<<"\n";
    cout<<"  total_transmitted = "<<total_transmitted<<"\n";
    cout<<"  total_dropped = "<<total_dropped<<"\n";
    cout<<"  lost_fraction (Хар1) = "<<lost_fraction<<"\n";
    cout<<"  avg_attempts_per_packet (Хар2) = "<<avg_attempts<<"\n";
    cout<<"  avg_queue_len_per_adapter (mean over hosts) = "<<mean_avg_queue_len<<"\n";
    cout<<"  max_queue_len_over_hosts (Хар3 max) = "<<max_queue_overall<<"\n";
    cout<<"  avg_queue_len_each_host:\n";
    for (int i=0;i<N;i++){
        cout<<"    host "<<i<<": avg="<<avg_queue_len[i]<<" max="<<max_queue_len[i]<<"\n";
    }
    cout<<"  channel_util (Хар4) = "<<channel_util<<"\n";

    // --- CSV LOGGING ---
    if (!csv_file.empty()) {
        bool exists = false;
        {
            std::ifstream f(csv_file);
            exists = f.good();
        }
        std::ofstream out(csv_file, std::ios::app);
        if (!exists) {
            out << "N,alpha,simtime,B,l_bytes,L,qlimit,channel_util,loss_fraction,avg_attempts,mean_avg_queue,max_queue\n";
        }
        out << N << ","
            << alpha << ","
            << simTime << ","
            << B << ","
            << l_bytes << ","
            << L << ","
            << qlimit << ","
            << channel_util << ","
            << lost_fraction << ","
            << avg_attempts << ","
            << mean_avg_queue_len << ","
            << max_queue_overall
            << "\n";
        out.close();
    }

    return 0;
}
