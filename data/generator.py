import random

# A simple script to simulate a mean-reverting price walk hello
fair_value = 150.0
with open("./historical_ticks.csv", "w") as f:
    f.write("timestamp,action,side,price,qty\n")
    for i in range(1000):
        # Move fair value slightly
        fair_value += random.uniform(-0.05, 0.05)
        
        # Add a bid 0.1 below and an ask 0.1 above
        f.write(f"10:00:{i:03d},ADD,BID,{fair_value-0.1:.2f},10\n")
        f.write(f"10:00:{i:03d},ADD,ASK,{fair_value+0.1:.2f},10\n")