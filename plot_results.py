import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv("results.csv")  
# Ожидаемые столбцы: N, alpha, channel_util, loss_fraction, max_queue

plt.figure()
for a in sorted(df.alpha.unique()):
    sub = df[df.alpha == a]
    plt.plot(sub.N, sub.channel_util, label=f"alpha={a}")
plt.xlabel("N")
plt.ylabel("Channel Utilization")
plt.title("Зависимость channel_util от N")
plt.legend()
plt.grid()
plt.savefig("channel_util_vs_N.png", dpi=200)

plt.figure()
for a in sorted(df.alpha.unique()):
    sub = df[df.alpha == a]
    plt.plot(sub.N, sub.max_queue, label=f"alpha={a}")
plt.xlabel("N")
plt.ylabel("Max queue length")
plt.title("Зависимость максимальной длины очереди от N")
plt.legend()
plt.grid()
plt.savefig("max_queue_vs_N.png", dpi=200)

plt.figure()
for a in sorted(df.alpha.unique()):
    sub = df[df.alpha == a]
    plt.plot(sub.N, sub.loss_fraction, label=f"alpha={a}")
plt.xlabel("N")
plt.ylabel("Loss fraction")
plt.title("Зависимость потерь от N")
plt.legend()
plt.grid()
plt.savefig("loss_vs_N.png", dpi=200)

print("Готово: графики созданы.")
