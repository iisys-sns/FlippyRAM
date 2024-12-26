import re
from datetime import datetime
import os

script_dir = os.path.dirname(os.path.abspath(__file__))
tmp_path = os.path.join(script_dir, '../../data/tmp/temp-addr-drama.log')
dramaAddr_path = os.path.join(script_dir, '../../data/drama-functions.txt')
pool_path = os.path.join(script_dir, '../../data/reaf-pool.txt')

def handleData(file):
    f = open(file, "r")
    data = f.read()
    f.close()
    chunks = re.split(r' \(\w+: \d+%\) ', data)
    percentages = re.findall(r'\(\w+: (\d+)%\)', data)
    combinations = []
    for chunk in chunks:
        numbers = re.findall(r'\d+', chunk)
        combinations.append([int(num) for num in numbers])


    f = open(dramaAddr_path,"w")
    d = datetime.today().strftime('%Y-%m-%d %H:%M:%S')
    f.write(d)
    f.close()
    f = open(dramaAddr_path,"a")
    firstLine = True
    dict={}
    combinations = [combo for combo in combinations if combo]
    for combo, percentage in zip(combinations, percentages):
        percentage = int(percentage)
        results = [2 ** num for num in combo]
        total = sum(results)
        dict.update({hex(total) : percentage})
        if(firstLine == True):
            f.write(f"\nindex: {combo}, addressing function: {hex(total)}, Percentage: {percentage}%\n")
            firstLine = False
        else:
            f.write(f"index: {combo}, addressing function: {hex(total)}, Percentage: {percentage}%\n")
    top_functions = sorted(dict, key=dict.get, reverse=True)
    with open(pool_path, 'w') as file:
        file.write(','.join(top_functions))

handleData(tmp_path)
