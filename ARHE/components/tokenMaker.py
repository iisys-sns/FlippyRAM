import os
import math

def crypto_rand_secure(min_val, max_val):
    range_val = max_val - min_val
    if range_val < 1:
        return min_val

    log = math.ceil(math.log(range_val, 2))
    bytes_len = (log // 8) + 1
    bits = log + 1
    filter_val = (1 << bits) - 1

    while True:
        rnd = int.from_bytes(os.urandom(bytes_len), 'big') & filter_val
        if rnd <= range_val:
            break

    return min_val + rnd

def get_token(length):
    token = ""
    code_alphabet = "0123456789abcdefghijklmnopqrstuvwxyz"
    max_val = len(code_alphabet) - 1

    for _ in range(length):
        token += code_alphabet[crypto_rand_secure(0, max_val)]

    return token

script_dir = os.path.dirname(os.path.abspath(__file__))
token_path = os.path.join(script_dir, '../data/token.txt')

token = get_token(10)
with open(token_path, 'w') as file:
    file.write(token)