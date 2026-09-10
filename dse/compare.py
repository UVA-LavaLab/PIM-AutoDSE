import pandas as pd


energy_opt_df = pd.read_csv("histogram_energy_opt_dse.csv")
perf_opt_df = pd.read_csv("histogram_perf_opt_dse.csv")



counter = 0

for row in range(len(energy_opt_df)):
    eg_row = list(energy_opt_df.iloc[row].values)
    for other_row in range(len(energy_opt_df)):
        perf_row = list(perf_opt_df.iloc[other_row].values)

        if perf_row[0] != eg_row[0]:
            continue

        print(perf_row, eg_row)
        if eg_row == perf_row:
            continue
        counter += 1

        print("="* 50)
        print("Energy row")
        print(eg_row)

        print("Perf row")
        print(perf_row)


print("Counter:", counter)
