import numpy as np
import random
import struct
import os


def ensure_uint64_random(x):
    """Clamp value into uint64_t range, but if overflow, use random uint64 instead."""
    if x < 0 or x >= 2**64:
        return np.random.randint(0, 2**64, dtype=np.uint64)
    return int(x)

def build_key_sequence(num_keys, num_records, mode, zipf_param=None):
    keys = list(range(1, num_keys + 1))
    remaining = num_records - num_keys

    if remaining < 0:
        raise ValueError("num_records must be >= num_keys")

    if mode == "uniform":
        additional = np.random.randint(1, num_keys + 1, size=remaining)

    elif mode == "zipf":
        weights = np.arange(1, num_keys + 1, dtype=np.float64) ** (-zipf_param)
        weights /= weights.sum()
        additional = np.random.choice(
            np.arange(1, num_keys + 1),
            size=remaining,
            p=weights
        )
    else:
        raise ValueError("Invalid key_mode")

    keys.extend(additional)
    random.shuffle(keys)
    return keys



def generate_value_lognormal(mu, sigma):
    v = np.random.lognormal(mean=mu, sigma=sigma)
    return ensure_uint64_random(v)

def generate_value_gamma(mu, shape_k):
    theta = mu / shape_k
    v = np.random.gamma(shape_k, theta)
    return ensure_uint64_random(v)


def generate_dataset(
    num_keys,
    num_records,
    key_mode,       # 'uniform' or 'zipf'
    zipf_param,     # only used for zipf
    mu_range,
    value_mode,     # 'lognormal' or 'gamma'
    sigma=None,     # for lognormal
    gamma_k=None,   # for gamma
    output_file=""
):
    a, b = mu_range
    key_mu = np.random.uniform(a, b, num_keys + 1)

    keys = build_key_sequence(
        num_keys=num_keys,
        num_records=num_records,
        mode=key_mode,
        zipf_param=zipf_param
    )

    os.makedirs(os.path.dirname(output_file), exist_ok=True)

    with open(output_file, 'wb') as f:
        for key in keys:
            mu = key_mu[key]

            if value_mode == "lognormal":
                value = generate_value_lognormal(mu, sigma)

            elif value_mode == "gamma":
                value = generate_value_gamma(mu, gamma_k)

            else:
                raise ValueError("Invalid value_mode")

            f.write(struct.pack('QQ', value, key))


if __name__ == "__main__":
    num_keys_zipf = 100000
    num_keys_uniform=1000
    num_records = 10000000

    generate_dataset(
        num_keys=num_keys_uniform,
        num_records=num_records,
        key_mode="uniform",
        zipf_param=None,
        mu_range=(10.0, 32.0),
        value_mode="lognormal",
        sigma=1.0,
        output_file="./Dataset/uniform-lognormal.bin"
    )
    print("Generated uniform-lognormal.bin")

    generate_dataset(
        num_keys=num_keys_uniform,
        num_records=num_records,
        key_mode="uniform",
        zipf_param=None,
        mu_range=(10.0, 5000.0),
        value_mode="gamma",
        gamma_k=3.0,
        output_file="./Dataset/uniform-gamma.bin"
    )
    print("Generated uniform-gamma.bin")

    generate_dataset(
        num_keys=num_keys_zipf,
        num_records=num_records,
        key_mode="zipf",
        zipf_param=1.5,
        mu_range=(10.0, 32.0),
        value_mode="lognormal",
        sigma=1.0,
        output_file="./Dataset/zipf-lognormal.bin"
    )
    print("Generated zipf-lognormal.bin")

    generate_dataset(
        num_keys=num_keys_zipf,
        num_records=num_records,
        key_mode="zipf",
        zipf_param=1.5,
        mu_range=(10.0, 5000.0),
        value_mode="gamma",
        gamma_k=3.0,
        output_file="./Dataset/zipf-gamma.bin"
    )
    print("Generated zipf-gamma.bin")

    for i in range(10, 31):
        alpha = 0.1 * i
        generate_dataset(
            num_keys=num_keys_zipf,
            num_records=num_records,
            key_mode="zipf",
            zipf_param=alpha,
            mu_range=(10.0, 32.0),
            value_mode="lognormal",
            sigma=1.0,
            output_file=f"./Dataset/zipf-lognormal-{i}.bin"
        )
        print(f"Generated zipf-lognormal-{i}.bin")