import subprocess
import re
import sys

def run_sim(N, alpha, simtime=10, B=100, L=30, l=1500, qlimit=-1):
    """
    Запуск бинарника csma_sim и извлечение channel_util.
    Возвращает float channel_util или None если ошибка.
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

    # ищем строку вида:  "channel_util (Хар4) = 0.123456"
    m = re.search(r"channel_util\s*\(.*?\)\s*=\s*([0-9.]+)", output)
    if not m:
        print("Не найден channel_util в выводе")
        print(output)
        return None
    return float(m.group(1))


def find_max_N(alpha, util_threshold=0.5, max_search=500):
    """
    Находит максимальное N, при котором channel_util <= util_threshold.
    Возвращает (N_found, channel_util).
    """

    low, high = 1, max_search
    best_N = 0
    best_util = 0.0

    while low <= high:
        mid = (low + high) // 2
        util = run_sim(mid, alpha)

        if util is None:
            print("ОШИБКА: util None при N =", mid)
            break

        print(f"N={mid}, util={util:.4f}")

        if util <= util_threshold:
            best_N = mid
            best_util = util
            low = mid + 1
        else:
            high = mid - 1

    return best_N, best_util


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Использование: python3 find_max_n.py <alpha>")
        sys.exit(1)

    alpha = float(sys.argv[1])
    print(f"Поиск максимального N при channel_util <= 0.5 для alpha={alpha}")

    N, util = find_max_N(alpha)
    print("=====================================")
    print(f"Максимальное N = {N}")
    print(f"channel_util = {util:.5f}")
