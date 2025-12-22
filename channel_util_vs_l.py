import subprocess
import re
import sys
import matplotlib.pyplot as plt

def run_sim(N, alpha, l_bytes, simtime=10, B=100, L=30, qlimit=-1):
    cmd = [
        "./csma_sim",
        "--N", str(N),
        "--alpha", str(alpha),
        "--simtime", str(simtime),
        "--B", str(B),
        "--L", str(L),
        "--l", str(l_bytes),
        "--qlimit", str(qlimit)
    ]

    try:
        output = subprocess.check_output(cmd, stderr=subprocess.STDOUT).decode()
    except Exception as e:
        print("Ошибка при запуске:", e)
        return None

    # Парсим строку вывода вида: "channel_util (Хар4) = 0.123456"
    m = re.search(r"channel_util\s*\(.*?\)\s*=\s*([0-9.]+)", output)
    if not m:
        print("Не найден channel_util в выводе")
        return None
    return float(m.group(1))


def analyze_channel_util_vs_l(N, alpha, l_min=100, l_max=5000, l_step=100):
    """
    Анализирует зависимость channel_util от длины пакета l.
    
    Параметры:
    - N: количество хостов (фиксированное)
    - alpha: интенсивность поступления пакетов (фиксированное)
    - l_min: минимальная длина пакета в байтах
    - l_max: максимальная длина пакета в байтах
    - l_step: шаг изменения длины пакета
    """
    l_values = []
    util_values = []
    
    # print(f"Анализ зависимости channel_util от длины пакета l")
    # print(f"Параметры: N={N}, alpha={alpha}")
    # print(f"Диапазон l: {l_min} - {l_max} байт, шаг: {l_step}")
    # print("=" * 60)
    
    l = l_min
    while l <= l_max:
        # Запускаем симуляцию для текущего размера пакета
        print(f"Запуск симуляции: l={l} байт...", end=" ")
        util = run_sim(N, alpha, l)
        
        if util is None:
            print("ОШИБКА")
            l += l_step
            continue
        
        l_values.append(l)
        util_values.append(util)
        print(f"channel_util={util:.4f}")
        
        l += l_step
    
    # print("=" * 60)
    # print("\nРезультаты:")
    # print(f"{'l (байты)':<12} {'channel_util':<15}")
    # print("-" * 30)
    # for l, util in zip(l_values, util_values):
    #     print(f"{l:<12} {util:<15.6f}")
    
    return l_values, util_values


def plot_results(l_values, util_values, N, alpha, output_file="channel_util_vs_l.png"):
    plt.figure(figsize=(10, 6))
    plt.plot(l_values, util_values, 'b-o', markersize=4, linewidth=2)
    plt.xlabel("Длина пакета l (байты)", fontsize=12)
    plt.ylabel("Channel Utilization", fontsize=12)
    plt.title(f"Зависимость channel_util от длины пакета l\nN={N}, alpha={alpha}", fontsize=14)
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(output_file, dpi=200)
    print(f"\nГрафик сохранён в {output_file}")


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Использование: python3 channel_util_vs_l.py <N> <alpha> [l_min] [l_max] [l_step]")
        print("Пример: python3 channel_util_vs_l.py 50 50 100 5000 100")
        sys.exit(1)
    
    N = int(sys.argv[1])
    alpha = float(sys.argv[2])
    
    # Опциональные параметры
    l_min = int(sys.argv[3]) if len(sys.argv) > 3 else 100
    l_max = int(sys.argv[4]) if len(sys.argv) > 4 else 5000
    l_step = int(sys.argv[5]) if len(sys.argv) > 5 else 100
    
    l_values, util_values = analyze_channel_util_vs_l(N, alpha, l_min, l_max, l_step)
    
    if l_values:
        plot_results(l_values, util_values, N, alpha)

