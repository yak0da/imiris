import subprocess
import re
import sys

def run_sim(N, alpha, simtime=30, B=100, L=30, l=1500, qlimit=-1):
    """
    Запуск бинарника csma_sim и извлечение max_queue_len_over_hosts.
    """
    cmd = [
        "./csma_sim",
        "--N", str(N),
        "--alpha", str(alpha),
        "--simtime", str(simtime),
        "--B", str(B),
        "--L", str(L),
        "--l", str(l),
        "--qlimit", str(qlimit)
    ]

    try:
        output = subprocess.check_output(cmd, stderr=subprocess.STDOUT).decode()
    except Exception as e:
        print("Ошибка при запуске:", e)
        return None

    m = re.search(r"max_queue_len_over_hosts.*?=\s*([0-9]+)", output)
    if not m:
        print("Не найден max_queue_len")
        print(output)
        return None
    return int(m.group(1))


def is_queue_stable(N, alpha, T1=20, T2=40, threshold=2):
    """
    Проверяет, ограничена ли очередь:
    maxQ(T2) - maxQ(T1) <= threshold
    """
    q1 = run_sim(N, alpha, simtime=T1)
    q2 = run_sim(N, alpha, simtime=T2)

    if q1 is None or q2 is None:
        return False

    print(f"    N={N}: maxQ({T1})={q1}, maxQ({T2})={q2}")

    return (q2 - q1) <= threshold


def find_max_stable_N(alpha, max_search=500):
    """
    Ищет максимальное N, при котором очередь ограничена.
    """
    best_N = 0
    low, high = 1, max_search

    while low <= high:
        mid = (low + high) // 2
        print(f"Проверка N={mid}...")
        if is_queue_stable(mid, alpha):
            best_N = mid
            low = mid + 1
        else:
            high = mid - 1

    return best_N


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Использование: python3 find_stable_queue.py <alpha>")
        sys.exit(1)

    alpha = float(sys.argv[1])

    print(f"Поиск максимального стабильного N для alpha={alpha}")
    N = find_max_stable_N(alpha)
    print("=====================================")
    print(f"Максимальное стабильное N = {N}")
